/*
 * The supervision of a peer's rate (wlan/peerrate.c): every 10 ms the transmit statistics of the
 * peer's block acknowledgement agreements are added to the rate control's window, and the rate
 * selection decides whether to move to another rate. The functions keep the firmware's placeholder
 * names ("macTxNotify*Frame"); what they do is not about frames sent. The second half of the
 * file is a small store of per-channel calibration values.
 */
#ifndef ROBIN_PEERRATE_H
#define ROBIN_PEERRATE_H

#include <stdint.h>

struct WlanCalibrationChannelValues;
struct WlanConnection;
struct WlanPeerLink;
struct WlanRateControl;
struct WlanRateLevel;
struct WlanRateWindow;
struct WlanTxStatus;

int macTxNotifyFilterPacket(struct WlanConnection *connection);			/* 0x332ec */
int macTxNotifyHandleFrame(struct WlanConnection *connection);				/* 0x33320 */
int macTxNotifyProcessFrame(struct WlanConnection *connection);			/* 0x3333c */
int macTxNotifyUpdateFrame(struct WlanConnection *connection);				/* 0x3337c */
void macTxNotifyConfigureFrame(struct WlanConnection *connection);			/* 0x333ce */
void wlanBaNotifyAdded(struct WlanConnection *connection, int hardwareSlot);		/* 0x333f6 */
void wlanBaNotifyDeleted(struct WlanConnection *connection, int hardwareSlot);		/* 0x33420 */
void macTxNotifyPrepareFrame(struct WlanPeerLink *link);				/* 0x3346a */
void macTxNotifyDispatchFrame(struct WlanConnection *connection);			/* 0x3351c */
void macTxNotifyParseFrame(struct WlanConnection *connection);				/* 0x33528 */
void macTxNotifyBuildFrame(uint32_t argument);						/* 0x33534 */
unsigned int macTxNotifyComputeFrame(void);						/* 0x3355c */
void macTxNotifyQueryFrame(struct WlanConnection *connection);				/* 0x33570 */
void macTxNotifyResetFrame(struct WlanConnection *connection, struct WlanTxStatus *status); /* 0x33592 */
void hostCmd007f(struct WlanConnection *connection, uint8_t *rate, uint8_t *flags);	/* 0x335b0 */
int macTxNotifyApplyFrame(struct WlanConnection *connection);				/* 0x3360a */
int macTxNotifyResolveFrame(unsigned int index, const uint32_t *value);		/* 0x3361c */
int macTxNotifyScheduleFrame(unsigned int index, uint32_t *value);			/* 0x3362a */
int macTxNotifyNotifyFrame(unsigned int index, const struct WlanCalibrationChannelValues *values); /* 0x33638 */
int macTxNotifyFormatFrame(unsigned int index, struct WlanCalibrationChannelValues *values); /* 0x33662 */
void macTxNotifyInspectFrame(void);							/* 0x33692 */
void macTxNotifyTrackFrame(void);							/* 0x336a0 */

/* The rate selection that decides (0x3c03c to 0x3d4c6, not converted yet). */
int wlanPsAcquireBuffer(struct WlanConnection *connection);				/* 0x3c97c */
int wlanPsManageBuffer(struct WlanConnection *connection);				/* 0x3c97a */
unsigned int wlanPsReleaseBuffer(void);						/* 0x3c9ba */
int wlanPsHandlePacket(struct WlanConnection *connection);				/* 0x3cc48 */
int wlanPsProcessPacket(struct WlanRateControl *rateControl, struct WlanTxStatus *status); /* 0x3cd54 */
unsigned int wlanPsComputePacket(void);						/* 0x3cff4 */
int wlanPsUpdateFrame(unsigned int rate, uint8_t *wide, uint8_t *index);		/* 0x3d402 */
int wlanPsConfigureFrame(const struct WlanRateLevel *level);				/* 0x3d45c */
void wlanPsValidateFrame(struct WlanRateWindow *window);				/* 0x3d480 */

#endif /* ROBIN_PEERRATE_H */
