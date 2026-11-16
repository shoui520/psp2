/*
 * Power-save control (0x168e0..0x17144): the roles power save is decided
 * for, the control task's event bits, the wake timer that ends a sleep
 * between beacons, the traffic a beacon's TIM announces, and the queries the
 * MAC, the host interface and Bluetooth make of the power-save state. The
 * filename is inferred; no source filename survives.
 */
#include <stdint.h>
#include "robin/coex.h"
#include "robin/txhold.h"
#include "robin/hostcmd.h"
#include "robin/hostps.h"
#include "robin/conn.h"
#include "robin/join.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/psctl.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/slot.h"
#include "robin/stapssm.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/timer.h"
#include "robin/txring.h"
#include "robin/wlantask.h"

/* The control task's event: event bits are pending (wlanRaiseEvent()). */
#define WLAN_CB_EVENT_PENDING_BITS	0x4000000

/* Control-task events (wlanRaiseEvent()); numbers only, few are understood. */
#define WLAN_PS_EVENT_BT_SHARE		3	/* Bluetooth takes the antenna: end the sleep sequence */
#define WLAN_PS_EVENT_CONFIRM_TIMEOUT	10	/* "SleepConfirmTmr" expired */
#define WLAN_PS_EVENT_WAKE		11	/* the beacon announced traffic: wake */
#define WLAN_PS_EVENT_SLEEP		12	/* no traffic: sleep again */
#define WLAN_PS_EVENT_NULL_TIMEOUT	15	/* "NullPktDoneTmr" expired */
#define WLAN_PS_EVENT_WAKE_TIMER	20	/* the wake timer expired */
#define WLAN_PS_EVENT_PERIOD_SLEEP	23	/* the MAC may sleep for the period */

/* wlanSessionId bits wlanPsEndSession() clears. */
#define WLAN_SESSION_STATION		0x0010
#define WLAN_SESSION_AP			0x0300

/* WlanStaPsMachine.hostState: wlanPsSetPowerState(). */
#define WLAN_PS_POWER_AWAKE		2
#define WLAN_PS_POWER_DOZE		4
#define WLAN_PS_POWER_ASLEEP		5

/* hostCmdCurrentCode(): the control task's state while it scans. */
#define WLAN_CB_STATE_SCAN		0x21

/* The default wake timer (wlanPsInit()): 50 beacons, 5 s when idle. */
#define WLAN_PS_WAKE_BEACONS		50
#define WLAN_PS_IDLE_WAKE_DELAY		5000

uint32_t wlanPsOverride __attribute__((section(".itcm.rw"))) = 1;			/* 0x54b48 */
uint32_t wlanPsSleepLeft __attribute__((section(".itcm.rw"))) = 1;			/* 0x54b4c */
uint32_t wlanPsIeeeActive __attribute__((section(".itcm.rw")));		/* 0x54b50 */
uint32_t wlanPsDtimPolicy __attribute__((section(".itcm.rw"))) = 1;		/* 0x54b54 */
uint32_t wlanPsSavedDtimPolicy __attribute__((section(".itcm.rw"))) = 1;		/* 0x54b58 */
struct OsaSemaphore *wlanPsResetDone __attribute__((section(".itcm.rw")));	/* 0x54b5c */
uint32_t wlanPsWakeTimer __attribute__((section(".itcm.rw")));			/* 0x54b60 */
struct WlanInitStage wlanPsInitStage __attribute__((section(".itcm.rw")));	/* 0x54b64 */
uint16_t wlanPsNullWindow __attribute__((section(".itcm.rw"))) = 0x000a;		/* 0x54b6c */

uint32_t wlanPendingEvents[2] __attribute__((section(".itcm.bss")));		/* 0x56280 */
uint16_t wlanPsBeaconInterval __attribute__((section(".itcm.bss")));		/* 0x56288 */
uint16_t wlanPsWakeBeacons __attribute__((section(".itcm.bss")));		/* 0x5628a */
uint16_t wlanPsWakeDtims __attribute__((section(".itcm.bss")));		/* 0x5628c */
uint16_t wlanPsIdleWakeDelay __attribute__((section(".itcm.bss")));		/* 0x5628e */
uint16_t wlanPsWakeClasses __attribute__((section(".itcm.bss")));		/* 0x56290 */
uint16_t wlanPsListenBeacons __attribute__((section(".itcm.bss")));		/* 0x56292 */
uint8_t wlanPsWakeOnDtim __attribute__((section(".itcm.bss")));		/* 0x56294 */
uint32_t wlanPsWakeWindow __attribute__((section(".itcm.bss")));		/* 0x56298 */
uint32_t wlanPsCalibrationInterval __attribute__((section(".itcm.bss")));	/* 0x5629c */
uint16_t wlanPsAltModeActive __attribute__((section(".itcm.bss")));		/* 0x562a0 */
uint16_t wlanPsAltModeRequest __attribute__((section(".itcm.bss")));		/* 0x562a2 */
uint32_t wlanPsParameter __attribute__((section(".itcm.bss")));		/* 0x562a4 */

static TX_SEMAPHORE wlanPsResetDoneStorage __attribute__((section(".itcm.bss")));	/* 0x568a0 */

/* Not converted yet; provisional names. */
uint16_t wlanPsSessionTimeout __attribute__((section(".itcm.bss")));			/* 0x568cc */
extern uint32_t wlanPsWakeAdvance;			/* 0x56260 */
extern struct WlanPsHostConfig wlanPsHostConfig;	/* 0x568be */
uint16_t sleepHostWakeCount __attribute__((section(".itcm.bss")));			/* 0x5622e */
uint32_t sleepHostWakeMargin __attribute__((section(".itcm.bss")));			/* 0x56234 */
uint32_t sleepHostClockParameter __attribute__((section(".itcm.bss")));			/* 0x5623c */
extern uint32_t wlanIbssPsActive;			/* 0x593d4 */
extern const uint16_t rfCalibrationTable[];		/* 0x53d56 */
int sysRefClockProfile(void);			/* 0x26842 */
void clockHostSetMax(void);			/* 0x26ae0 */

/* 0x168e0: the AP role is the only one present. */
int wlanHasApRole(void)
{
	return wlanRoleMaskIs(3);
}

/* 0x168ea: only the "other" role is present. */
int wlanRoleOtherOnly(void)
{
	return wlanRoleMaskIs(1);
}

/* 0x168f4: only an IBSS is present. */
int wlanRoleIbssOnly(void)
{
	return wlanRoleMaskIs(2);
}

/* 0x168fe */
uint32_t wlanPsGetOverride(void)
{
	return wlanPsOverride;
}

/* 0x16904: the host opened a session and neither coexistence mode runs. */
int wlanPsSessionOpen(void)
{
	return wlanSessionId != 0 && !wlanCoexUseP2pMode() && !wlanCoexUseStationMode();
}

/* 0x16926: power save is on: the station is not held awake. */
int wlanPsEnabled(void)
{
	return wlanPsOverride == 0;
}

/* 0x16936 */
int wlanIbssPsEnabled(void)
{
	return wlanRoleIbssOnly() != 0 && wlanPsEnabled();
}

/* 0x16950 */
int wlanApPsEnabled(void)
{
	return wlanHasApRole() != 0 && wlanPsEnabled();
}

/* 0x1696a */
int wlanPsRecalibrate(void)
{
	sleepCalibrate();
	return 1;
}

/* 0x16976: the host's power-save parameters before it sets any. */
void wlanPsHostConfigDefaults(void)
{
	uint16_t spacing;

	wlanPsPollWindow = 500;
	wlanSessionId = 0;
	wlanPsSessionTimeout = 10;
	wlanPsHostConfig.holdSeconds[0] = 30;
	wlanPsHostConfig.holdSeconds[1] = 0;
	wlanPsHostConfig.dtimPolicy[0] = 1;
	wlanPsHostConfig.dtimPolicy[1] = 0;
	wlanPsHostConfig.nullWindow[0] = 10;
	wlanPsHostConfig.nullWindow[1] = 0;
	spacing = wlanJoinListenInterval();
	wlanPsHostConfig.headerSpacing[0] = spacing;
	wlanPsHostConfig.headerSpacing[1] = spacing >> 8;
	wlanPsHostConfig.reserved8[0] = 4;
	wlanPsHostConfig.reserved8[1] = 0;
	wlanPsHostConfig.btShare[0] = 3;
	wlanPsHostConfig.btShare[1] = 0;
	wlanPsHostConfig.pollWindow[0] = wlanPsPollWindow;
	wlanPsHostConfig.pollWindow[1] = wlanPsPollWindow >> 8;
	wlanPsBtShareMode = 0;
}

/* 0x169c4: the sleep clock, the calibration and the power-save parameters. */
int wlanPsInit(void)
{
	sleepClockInit();
	wlanPsSleepSequence = 0;
	wlanConnectionPhase = 0;
	wlanHostSleepEndOnWake = 0;
	wlanPsHostActive = 0;
	wlanPsWakeAdvance = 345;
	sleepWakeLatency = rfCalibrationTable[10 + sysRefClockProfile()];
	sleepCalibrating = 0;
	sleepCalibrationCycles = 1;
	sleepCalibrationSamples = 1;
	sleepCalibrationTime = 0;
	sleepCalibrationPeriod = 1000;
	sleepKeepWakeTiming = 1;
	sleepHostWakeCount = 1;
	sleepHostWakeMargin = 50;
	sleepCalibrationInterval = 20000;
	sleepCalibrationDivisor = 0x683;
	sleepCalibrationHeld = 1;
	wlanPendingEvents[0] = 0;
	wlanPendingEvents[1] = 0;
	wlanPsParameter = 0;
	sleepHostClockParameter = 0x1a0a;
	wlanPsIdleWakeDelay = WLAN_PS_IDLE_WAKE_DELAY;
	wlanPsWakeBeacons = WLAN_PS_WAKE_BEACONS;
	wlanPsWakeDtims = 0;
	wlanPsListenBeacons = 0;
	wlanPsBeaconInterval = 0;
	wlanPsWakeClasses = 0;
	wlanPsAltModeActive = 0;
	wlanPsCalibrationInterval = 1000;
	wlanPsWakeWindow = 1;
	wlanStaPsReset();
	wlanPsResolveBuffer();
	wlanPsHostConfigDefaults();
	sleepCalibrate();
	return 0;
}

/* 0x16a4c: the power-save machines, and wlanPsInit() as an init stage. */
int wlanPsSetup(void)
{
	wlanPsResetDone = (struct OsaSemaphore *)&wlanPsResetDoneStorage;
	osaSemaphoreInit(&wlanPsResetDone, 1, 12);
	wlanStaPsCreate();
	wlanPsSyncBuffer();
	macMgmtResolveDescriptor();
	wlanPsInit();
	wlanPsInitStage.run = wlanPsInit;
	return wlanInitStageRegister(&wlanPsInitStage);
}

/* 0x16a80: an event to the power-save machine of the role present. */
int wlanPsPostEvent(uint32_t event)
{
	if (wlanRoleIbssOnly())
		return mdevRomQueryState(&wlanIbssPsMachine, event, NULL);
	if (wlanHasApRole())
		return mdevRomQueryState(&wlanConnectionInfo(wlanConnectionByRole(2))->psMachine,
					 event, NULL);
	return mdevRomQueryState(&wlanStaPs, event, NULL);
}

/*
 * 0x16abe: raises event bit 0 to 47 for the control task. The task is woken
 * only when no event was pending.
 */
unsigned int wlanRaiseEvent(unsigned int bit)
{
	UINT posture;
	uint32_t low;
	uint32_t high;
	uint32_t oldLow;
	uint32_t oldHigh;

	if (bit < 32) {
		low = 1u << bit;
		high = 0;
	} else {
		low = 0;
		high = 1u << (bit - 32);
	}
	posture = sysIrqDisable();
	oldLow = wlanPendingEvents[0];
	oldHigh = wlanPendingEvents[1];
	wlanPendingEvents[0] = oldLow | low;
	wlanPendingEvents[1] = oldHigh | high;
	sysIrqRestore(posture);
	if ((oldLow | oldHigh) == 0)
		wlanPostConnectionEvent(wlanConnectionByRole(wlanHasApRole() ? 2 : 0),
					WLAN_CB_EVENT_PENDING_BITS);
	return oldLow | oldHigh;
}

/* 0x16b0a: "NullPktDoneTmr". */
void wlanPsApNullPktDoneTimerCb(ULONG input)
{
	wlanRaiseEvent(WLAN_PS_EVENT_NULL_TIMEOUT);
}

/* 0x16b0e */
void wlanPsWakeTimerExpired(uint32_t timer, uint32_t argument)
{
	if (timer != wlanPsWakeTimer)
		return;
	wlanPsWakeTimer = 0;
	wlanRaiseEvent(WLAN_PS_EVENT_WAKE_TIMER);
}

/* 0x16b20 */
void wlanPsCancelWakeTimer(void)
{
	osaCancelTimer(wlanPsWakeTimer);
	wlanPsWakeTimer = 0;
}

/*
 * 0x16b30: wakes the sleeping machine after wlanPsWakeBeacons beacons, or
 * wlanPsWakeDtims when a DTIM is awaited.
 */
void wlanPsStartBeaconWakeTimer(int dtim)
{
	uint32_t interval = 1000;
	UINT posture;
	uint32_t beacons;

	posture = sysIrqDisable();
	wlanPsCancelWakeTimer();
	if (wlanConnectionExists(WLAN_IF_IBSS) && wlanIbssPsActive != 0)
		goto out;
	if (((wlanStaPs.state == &wlanStaPs.sleep) |
	     (wlanIbssPsMachine.state == &wlanIbssPsMachine.sleep)) == 0)
		goto out;
	if (dtim == 0 || (beacons = wlanPsWakeDtims) == 0)
		beacons = wlanPsWakeBeacons;
	if (wlanPsBeaconInterval != 0)
		interval = wlanPsBeaconInterval;
	osaCreateTimer(wlanPsWakeTimerExpired, 0, beacons * interval, &wlanPsWakeTimer, 0);
out:
	sysIrqRestore(posture);
}

/* 0x16ba8 */
void wlanPsArmWakeTimer(void)
{
	wlanPsWakeOnDtim = 0;
	wlanPsStartBeaconWakeTimer(0);
}

/* 0x16bb4 */
void wlanPsArmIdleWakeTimer(void)
{
	if (wlanPsWakeTimer != 0)
		return;
	osaCreateTimer(wlanPsWakeTimerExpired, 0, wlanPsIdleWakeDelay, &wlanPsWakeTimer, 0);
}

/* 0x16bd6 */
void wlanPsArmWakeTimerIn(uint32_t delay)
{
	if (wlanPsWakeTimer != 0)
		return;
	osaCreateTimer(wlanPsWakeTimerExpired, 0, delay, &wlanPsWakeTimer, 0);
}

/* 0x16bf2 */
void wlanPsRearmWakeTimer(void)
{
	wlanPsStartBeaconWakeTimer(wlanPsWakeOnDtim);
}

/* 0x16bfa */
int wlanPsWakeTimerRunning(void)
{
	return wlanPsWakeTimer != 0;
}

/*
 * 0x16c06: the traffic a beacon's TIM announces, unicast frames (bits 0 and
 * 3) and group frames (bit 1). The sleeping machine wakes (event 11) for
 * traffic the host is to be woken for, and sleeps again (event 12) for none.
 */
void wlanPsBeaconTraffic(struct WlanConnection *connection, int unicast, int group, int shared)
{
	unsigned int event = 0;
	int ap = wlanHasApRole();
	int sleeping;
	uint32_t classes;
	UINT posture;
	int wake;

	if (wlanRoleOtherOnly() && wlanStaPs.state == &wlanStaPs.sleep)
		sleeping = 1;
	else
		sleeping = 0;
	classes = 0;
	if (!ap && wlanStaPs.state == &wlanStaPs.active)
		return;
	if ((wlanCoexUseP2pMode() == 1 || wlanCoexUseStationMode() == 1) && !ap) {
		posture = sysIrqDisable();
		if (!unicast) {
			wlanStaPs.headerTime = romGetTimerTimestamp();
			wlanRaiseEvent(WLAN_PS_EVENT_PERIOD_SLEEP);
		}
		if (wlanPsUapsdPartial() == 1) {
			if (group) {
				macTxSendPsPoll(connection, 0);
				wlanPsPeriodHold = 1;
			} else {
				wlanPsPeriodHold = 0;
			}
		}
		sysIrqRestore(posture);
		return;
	}
	if (shared && wlanCoexBtNeedsSharing() && !ap) {
		wlanStaPs.beaconWait = 1;
		wlanPsWakeOnDtim = 0;
		event = WLAN_PS_EVENT_WAKE;
		if (wlanHostSleepArmed())
			wlanMefConfig.queuedEvents = 1;
	} else if (sleeping | ap) {
		wake = 1;
		if (!ap && wlanStaPs.hostState != WLAN_PS_POWER_ASLEEP)
			wake = 0;
		if (sleeping) {
			wlanStaPs.beaconWait = group;
			wlanPsWakeOnDtim = unicast;
		}
		if ((unicast | group) == 0) {
			event = WLAN_PS_EVENT_SLEEP;
		} else {
			event = WLAN_PS_EVENT_WAKE;
			if (wake) {
				classes = (unicast ? 9 : 1) | (group ? 2 : 0);
				if (wlanHostSleepArmed() &&
				    !wlanHostSleepWakeMatch(classes,
							    __aeabi_uread4(wlanHostSleepActive->conditions) | 2) &&
				    !wlanMefClassEnabled(classes)) {
					event = 0;
					classes = 0;
					if (!ap)
						event = WLAN_PS_EVENT_SLEEP;
					if (!ap)
						wlanStaPs.beaconWait = 0;
				} else if (!ap) {
					wlanMefConfig.queuedEvents = 1;
				}
			}
		}
	}
	if (sleeping && !ap)
		wlanPsWakeClasses = classes;
	if (event != 0)
		wlanRaiseEvent(event);
}

/* 0x16ddc: a beacon that announces no traffic. */
void wlanPsBeaconIdle(struct WlanConnection *connection)
{
	if (wlanLocalState->powerSave != 1 &&
	    (wlanTxPowerSaveCount | wlanPsIeeeFlag | wlanPsDataPending) != 0)
		return;
	if (wlanCoexUseP2pMode() || wlanCoexUseStationMode())
		return;
	wlanStaPsShouldNotify();
	if (wlanRoleOtherOnly() && wlanPsEnabled() && (wlanHostSleepArmed() || wlanPsSessionOpen()) &&
	    (wlanPsSleepTimeout == 1 || (wlanSessionId != 0 && wlanPsSleepSequence != 0))) {
		if (!wlanCoexBtNeedsSharing()) {
			clockHostSetMax();
			wlanStaPs.beaconsSeen++;
			wlanPostConnectionEvent(connection, 1);
			return;
		}
		wlanPsSleepTimeout = 0;
		wlanPsSleepSequence = 0;
		wlanRaiseEvent(WLAN_PS_EVENT_BT_SHARE);
	}
	wlanStaPs.beaconsSeen++;
	wlanPsBeaconTraffic(connection, 0, 0, 1);
}

/* 0x16e86: "SleepConfirmTmr". */
void wlanPsSleepConfirmTimerCb(ULONG input)
{
	wlanRaiseEvent(WLAN_PS_EVENT_CONFIRM_TIMEOUT);
}

/* 0x16e8a: the power-save machine of the role present sleeps. */
int wlanPsAsleep(void)
{
	struct WlanConnectionInfo *info;
	struct RomHsmState *state;
	int asleep;
	int inSleep;

	if (wlanSlotHasCurrentAlias())
		return 1;
	if (wlanPsDtimPolicy == 0xffff)
		return 0;
	if (!wlanPsEnabled()) {
		if (wlanMefConfig.hostState == WLAN_HOST_ASLEEP ||
		    wlanMefConfig.hostState == WLAN_HOST_WAKING)
			return 0;
		return 1;
	}
	asleep = 0;
	if (wlanHasApRole()) {
		info = wlanConnectionInfo(wlanConnectionByRole(2));
		return info->psMachine.state == &info->psMachine.awake;
	}
	if (wlanRoleOtherOnly()) {
		state = wlanStaPs.state;
		inSleep = state == &wlanStaPs.sleep;
		if (!inSleep && wlanStaPs.servicePeriod != 0)
			inSleep = state == &wlanStaPs.confirm;
		asleep = (state == &wlanStaPs.periodic) | inSleep;
	} else if (wlanRoleIbssOnly()) {
		asleep = wlanIbssPsMachine.state == &wlanIbssPsMachine.sleep;
	}
	return asleep;
}

/* 0x16f46: the host cannot be reached now: the WLAN side sleeps. */
int wlanPsHostBlocked(int reason)
{
	if (wlanHostSleepArmed() || hostCmdCurrentCode() == WLAN_CB_STATE_SCAN ||
	    (reason == 5 && hostCmdNodeGet()))
		return 0;
	if (wlanPsAsleep())
		return 1;
	return wlanStaPs.servicePeriod != 0;
}

/* 0x16f80: a Bluetooth HCI packet of this type is to wait for the WLAN side. */
int wlanPsHciDeferred(int packetType)
{
	if (wlanPsOverride != 0 || (wlanSessionId | wlanConnectionPhase) == 0 ||
	    packetType == 10 || packetType == 11)
		return 1;
	return wlanPsHostBlocked(2);
}

/* 0x16fa4 */
uint8_t wlanPsGetPowerState(void)
{
	return wlanStaPs.hostState;
}

/* 0x16fac: the awake state is kept until the Sleep state is left. */
void wlanPsSetPowerState(uint8_t state)
{
	if (wlanStaPs.hostState == WLAN_PS_POWER_AWAKE && wlanPsSleepLeft == 0)
		return;
	wlanStaPs.hostState = state;
}

/* 0x16fc2: the power-save machine of the role present reached its sleep state. */
int wlanPsSleepStateReached(void)
{
	struct WlanConnectionInfo *info;

	if (wlanRoleOtherOnly()) {
		if (wlanCoexUseP2pMode() || wlanCoexUseStationMode())
			return wlanStaPs.state == &wlanStaPs.periodic;
		return wlanStaPs.state == &wlanStaPs.sleep;
	}
	if (wlanHasApRole()) {
		info = wlanConnectionInfo(wlanConnectionByRole(2));
		return info->psMachine.state == &info->psMachine.awake;
	}
	return wlanIbssPsMachine.state == &wlanIbssPsMachine.sleep;
}

/* 0x17024 */
uint32_t wlanStaPsServicePeriod(void)
{
	return wlanStaPs.servicePeriod;
}

/* 0x1702c: the control task is idle: sleep until the next timer is due. */
int wlanPsIdleSleep(void)
{
	struct WlanConnection *connection = wlanConnectionByRole(0);
	uint64_t deadline;
	uint32_t timers;

	if (connection != NULL && (connection->flags2 & 0x10) && wlanRoleOtherOnly())
		return wlanStaPsDeepSleep();
	timers = osaGetNextTimerDeadline(&deadline, 2, 1);
	if (timers != 0) {
		wlanPsSetPowerState(WLAN_PS_POWER_DOZE);
		return sleepEnter(&deadline, 0, 5000, timers, 0);
	}
	wlanPsSetPowerState(WLAN_PS_POWER_AWAKE);
	return sleepEnter(NULL, 0, 0, 0, 0);
}

/* 0x17084 */
void wlanPsRestoreDtimPolicy(void)
{
	wlanPsDtimPolicy = wlanPsSavedDtimPolicy;
}

/* 0x1708c: the host ends the station (bit 4) or access point (bits 8, 9) session. */
int wlanPsEndSession(struct WlanConnection *connection, uint32_t flags)
{
	int changed = 0;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	if (flags & WLAN_SESSION_STATION) {
		wlanSessionId &= ~WLAN_SESSION_STATION;
		if (!wlanHasApRole())
			changed = 1;
	}
	if (flags & WLAN_SESSION_AP) {
		wlanSessionId &= ~WLAN_SESSION_AP;
		info->sleep.mode = 0;
		if (wlanApPsEnabled())
			changed = 1;
	}
	if ((wlanSessionId & (WLAN_SESSION_STATION | WLAN_SESSION_AP)) == 0) {
		wlanPsSleepSequence = 0;
		wlanPsHostActive = 0;
	}
	return changed;
}

/* 0x170ec */
void wlanPsSetOverride(uint32_t awake)
{
	wlanPsOverride = awake;
}

/* 0x170f2 */
void wlanPsSetSleepLeft(uint32_t left)
{
	wlanPsSleepLeft = left;
}

/* 0x170f8 */
void wlanPsSetIeeeActive(uint32_t active)
{
	wlanPsIeeeActive = active;
}

/* 0x170fe */
uint32_t wlanPsInIeee(void)
{
	return wlanPsIeeeActive;
}
