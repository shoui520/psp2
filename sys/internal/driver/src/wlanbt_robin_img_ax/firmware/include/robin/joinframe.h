/*
 * The management frames of joining a BSS and leaving it (wlan/joinframe.c): the authentication,
 * (re)association and deauthentication a station sends, and what it does with the answers. The
 * functions keep the firmware's placeholder names, "macTxNotify*Frame" and "*Header".
 */
#ifndef ROBIN_JOINFRAME_H
#define ROBIN_JOINFRAME_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/wlan.h"

extern uint8_t wlanJoinHtCapability[28];		/* SQRAM 0xc0032154: the HT capability element the host gave */
extern uint8_t wlanAssocHtCapability[28];		/* SQRAM 0xc0032170: the copy of it for the association */

struct WlanConnection;
struct WlanJoinAssocRequest;
struct WlanJoinAuthRequest;
struct WlanJoinBss;
struct WlanTxStatus;

/* A deauthentication the management state asks for (15 bytes). */
struct WlanLeaveRequest {
	uint8_t peer[6];
	uint8_t channel[7];		/* what hostCmd001eChannel() gave: the channel and its flags */
	uint8_t reason[2];
};

void macTxNotifyManageFrame(struct WlanConnection *connection, uint16_t capability,
			    uint16_t associationId, const uint8_t *peer);		/* 0x336e0 */
int macTxNotifyAcquireFrame(OsaBuffer *buffer, const uint8_t *ssid, const uint8_t *rates,
			    const uint8_t *ies, int iesLength);				/* 0x33704 */
int macTxNotifyReleaseFrame(struct WlanConnection *connection,
			    const struct WlanJoinAssocRequest *request);		/* 0x33816 */
int macTxNotifySetupFrame(struct WlanConnection *connection,
			  const struct WlanJoinAssocRequest *request,
			  const struct WlanJoinBss *previous);				/* 0x33878 */
void macTxNotifyClearFrame(OsaBuffer *buffer);						/* 0x338e2 */
int macTxNotifyConvertFrame(OsaBuffer *buffer);						/* 0x33a5e */
void macTxNotifyVerifyFrame(void);							/* 0x33abc */
int macTxNotifySelectFrame(OsaBuffer *buffer);						/* 0x33acc */
void macTxNotifyAdjustFrame(int result, uint16_t statusCode);				/* 0x33c56 */
void macTxNotifyRecordFrame(uint8_t algorithm, int kind, const uint8_t *peer,
			    uint16_t statusCode);					/* 0x33c70 */
int macTxNotifyFilterFrame(struct WlanConnection *connection,
			   const struct WlanJoinAuthRequest *request);			/* 0x33c96 */
int macTxNotifyHandleHeader(struct WlanConnection *connection,
			    const struct WlanLeaveRequest *request, uint8_t notify);	/* 0x33d14 */
int macTxNotifyProcessHeader(OsaBuffer *buffer);					/* 0x33d3c */
int macTxNotifyUpdateHeader(OsaBuffer *buffer);						/* 0x33dee */
int macTxNotifyConfigureHeader(OsaBuffer *buffer);					/* 0x33eb6 */
int macTxNotifyCheckHeader(OsaBuffer *buffer);						/* 0x33ef8 */
int macTxNotifyValidateHeader(const WlanWirelessFrame *frame);					/* 0x33f72 */
void macTxNotifyPrepareHeader(unsigned int result);					/* 0x33fc0 */
int macTxNotifyDispatchHeader(uint32_t handle, int event, uint32_t argument);		/* 0x33fe0 */
int macTxNotifyParseHeader(uint32_t handle, const struct WlanTxStatus *status);		/* 0x3405e */
void macTxNotifyBuildHeader(struct WlanConnection *connection, const uint8_t *peer,
			    uint16_t reason, int (*callback)(int status, const uint8_t *address),
			    uint32_t timeout);						/* 0x34094 */
void macTxNotifyComputeHeader(int status, const uint8_t *address);			/* 0x34190 */
void macTxNotifyResetHeader(int status, const uint8_t *address);			/* 0x341c0 */

#endif /* ROBIN_JOINFRAME_H */
