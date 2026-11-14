/*
 * The host's sleep phase (0x17144..0x1760c); see wlan/hostps.c. The names
 * are inferred from use.
 */
#ifndef ROBIN_HOSTPS_H
#define ROBIN_HOSTPS_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"

extern uint16_t wlanPsPhaseDelay;
extern uint16_t wlanPsPollWindow;
extern uint32_t wlanPsHostActive;
extern uint32_t wlanPendingTimer;
extern uint32_t wlanPsHostModeTimer;

int wlanPsNoteHostActive(void);
uint32_t wlanPsHostIsActive(void);
void wlanPsHostModeTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsCancelHostModeTimer(void);
void wlanPsStartHostModeTimer(struct WlanConnection *connection);
void wlanPsPhaseTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsPhaseSuspend(void);
void wlanPsPhaseArm(struct WlanConnection *connection);
void wlanPsPhaseResume(struct WlanConnection *connection);
void wlanPsPhaseReset(void);
void wlanPsConfirmNow(void);
int wlanPsHostSleepReply(OsaBuffer *buffer);
int wlanPsHostSessionStart(void);
int wlanPsHciTraffic(OsaBuffer *buffer);
void wlanPsHostReady(void);
void wlanPsHostSleepEnd(void);
void wlanPsStageAdvance(void);
void wlanPsStageConfirm(void);

#endif /* ROBIN_HOSTPS_H */
