/*
 * The host's sleep phase (0x17144..0x1760c); see wlan/hostps.c. The names
 * are inferred from use.
 */
#ifndef ROBIN_HOSTPS_H
#define ROBIN_HOSTPS_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"

/* wlanPostConnectionEvent() event: the host sleeps. */
#define WLAN_EVENT_HOST_ASLEEP		0x00400000

extern uint16_t wlanPsPhaseDelay;
extern uint16_t wlanPsPollWindow;
extern uint32_t wlanPsHostActive;
extern uint32_t wlanPendingTimer;
extern uint32_t wlanPsHostModeTimer;

int wlanPsNoteHostActive(OsaBuffer *buffer);		/* the release of a PS_SLEEP event */
uint32_t wlanPsHostIsActive(void);
void wlanPsHostModeTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsCancelHostModeTimer(void);
void wlanPsStartHostModeTimer(struct WlanConnection *connection);
void wlanPsPhaseTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsPhaseSuspend(void);
void wlanPsPhaseArm(struct WlanConnection *connection);
void wlanPsPhaseResume(struct WlanConnection *connection);
void wlanPsPhaseReset(void);
int wlanPsHostModeExit(void);					/* 0: host power save mode was on */
int wlanPsHostModeEnter(const uint8_t *params);			/* 0: entered */
void wlanPsConfirmNow(void);
int wlanPsHostSleepReply(OsaBuffer *buffer);
int wlanPsHostSessionStart(OsaBuffer *buffer);		/* the release of a PS_AWAKE event */
void wlanPsHostAwakeAnnounce(struct WlanConnection *connection);
void wlanPortOpenNotify(struct WlanConnection *connection, uint32_t event);
void wlanPsHostCmdRespond(struct WlanConnection *connection);
int wlanPsHciTraffic(OsaBuffer *buffer);
void wlanPsHostReady(void);
void wlanPsHostSleepEnd(void);
void wlanPsStageAdvance(void);
void wlanPsStageConfirm(void);

#endif /* ROBIN_HOSTPS_H */
