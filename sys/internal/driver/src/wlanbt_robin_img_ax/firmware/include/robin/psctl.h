/*
 * Power-save control (0x168e0..0x17144): the roles power save is decided
 * for, the control task's event bits, the wake timer that ends a sleep
 * between beacons, the traffic a beacon announces, and the queries the MAC,
 * the host interface and Bluetooth make of the power-save state. The names
 * are inferred from use; see wlan/psctl.c.
 */
#ifndef ROBIN_PSCTL_H
#define ROBIN_PSCTL_H

#include <stddef.h>
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/threadx.h"
#include "robin/wlantask.h"

/* The power-save parameters (0x56288..0x562a4). */
extern uint16_t wlanPsBeaconInterval;		/* the wake timer's unit (us); 0: 1000 */
extern uint16_t wlanPsWakeBeacons;		/* the wake timer's length in beacons */
extern uint16_t wlanPsWakeDtims;		/* ... when a DTIM is awaited; 0: wlanPsWakeBeacons */
extern uint16_t wlanPsIdleWakeDelay;		/* wlanPsArmIdleWakeTimer() */
extern uint16_t wlanPsWakeClasses;		/* the wake classes of the last beacon's traffic */
extern uint16_t wlanPsListenBeacons;
extern uint8_t wlanPsWakeOnDtim;		/* the wake timer waits for a DTIM */
extern uint32_t wlanPsWakeWindow;		/* ms */
extern uint32_t wlanPsCalibrationInterval;	/* ms */
extern uint16_t wlanPsAltModeActive;		/* the period in TU */
extern uint16_t wlanPsAltModeRequest;
extern uint32_t wlanPsParameter;

/* The power-save state of the control task (0x54b48..0x54b6c). */
extern uint32_t wlanPsSleepLeft;		/* the Sleep state was left */
extern uint32_t wlanPsIeeeActive;		/* in the Ieee state */
extern uint32_t wlanPsDtimPolicy;
extern uint32_t wlanPsSavedDtimPolicy;
extern struct OsaSemaphore *wlanPsResetDone;
extern uint32_t wlanPsWakeTimer;
extern struct WlanInitStage wlanPsInitStage;
extern uint16_t wlanPsNullWindow;

extern uint32_t wlanPendingEvents[2];

/*
 * The IBSS power-save machine (0x5963c), made by the IBSS power-save code
 * (0x3c65a, not converted yet); only what wlan/psctl.c and wlan/hostps.c use.
 */
struct WlanIbssPsMachine {
	struct RomHsmState *state;	/* current state */
	struct RomHsmState root;
	struct RomHsmState off;		/* the state romHsmStart() enters */
	struct RomHsmState reserved28[3];
	struct RomHsmState sleep;
	struct RomHsmState reserved76[2];
	TX_TIMER confirmTimer;		/* "SleepConfirmTmr" */
};

_Static_assert(offsetof(struct WlanIbssPsMachine, sleep) == 64, "IBSS PS sleep state");
_Static_assert(offsetof(struct WlanIbssPsMachine, confirmTimer) == 100, "IBSS PS confirm timer");

extern struct WlanIbssPsMachine wlanIbssPsMachine;

uint32_t wlanPsGetOverride(void);
int wlanPsSessionOpen(void);
int wlanPsRecalibrate(void);
void wlanPsHostConfigDefaults(void);
int wlanPsInit(void);
int wlanPsSetup(void);
int wlanPsPostEvent(uint32_t event);
void wlanPsApNullPktDoneTimerCb(ULONG input);
void wlanPsWakeTimerExpired(uint32_t timer, uint32_t argument);
void wlanPsCancelWakeTimer(void);
void wlanPsStartBeaconWakeTimer(int dtim);
void wlanPsArmWakeTimer(void);
void wlanPsArmIdleWakeTimer(void);
void wlanPsArmWakeTimerIn(uint32_t delay);
void wlanPsRearmWakeTimer(void);
int wlanPsWakeTimerRunning(void);
void wlanPsBeaconTraffic(struct WlanConnection *connection, int unicast, int group, int shared);
void wlanPsBeaconIdle(struct WlanConnection *connection);
void wlanPsSleepConfirmTimerCb(ULONG input);
int wlanPsAsleep(void);
int wlanPsHostBlocked(int reason);
int wlanPsHciDeferred(int packetType);
uint8_t wlanPsGetPowerState(void);
void wlanPsSetPowerState(uint8_t state);
int wlanPsSleepStateReached(void);
uint32_t wlanStaPsServicePeriod(void);
int wlanPsIdleSleep(void);
void wlanPsRestoreDtimPolicy(void);
int wlanPsEndSession(struct WlanConnection *connection, uint32_t flags);
void wlanPsSetOverride(uint32_t awake);
void wlanPsSetSleepLeft(uint32_t left);
void wlanPsSetIeeeActive(uint32_t active);
uint32_t wlanPsInIeee(void);

#endif /* ROBIN_PSCTL_H */
