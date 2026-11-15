/*
 * The station's power-save state machine (0x143dc..), struct
 * WlanStaPsMachine in robin/stapssm.h. Active: power save is off; event 1
 * starts it. Below PowerSave, Confirm sends the PM=1 null frame and tells
 * the host (event 0x2000), and waits for both; Enter sets the PM bit when
 * the machine cannot sleep yet; Sleep sleeps between beacons and wakes for
 * traffic, PS-Poll and U-APSD service periods. Ieee drives the 802.11
 * power-management exchange with the access point. The filename is
 * inferred; no source filename survives.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/dmaring.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/pspoll.h"
#include "robin/hostps.h"
#include "robin/psctl.h"
#include "robin/pssm.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/slot.h"
#include "robin/stapssm.h"
#include "robin/mgmt.h"
#include "robin/subscribe.h"
#include "robin/wmm.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/threadx.h"
#include "robin/timer.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

/* Events posted to the machine; numbers only, few are understood. */
#define WLAN_STA_PS_EVENT_START		1	/* power save on */
#define WLAN_STA_PS_EVENT_STOP		2	/* power save off */
#define WLAN_STA_PS_EVENT_HOST_CONFIRMED 9
#define WLAN_STA_PS_EVENT_WAKE		17	/* wake for good */
#define WLAN_STA_PS_EVENT_LISTEN	31	/* the listen interval is over */
#define WLAN_STA_PS_EVENT_IEEE_ENTER	41
#define WLAN_STA_PS_EVENT_IEEE_EXIT	42
#define WLAN_STA_PS_EVENT_RESTART	46

/* macTxSendNullFrame() kinds. */
#define WLAN_NULL_KIND_STATION		16
#define WLAN_NULL_KIND_TRIGGER		18
#define WLAN_NULL_KIND_IEEE		19

/* mdevIoctl() of the SDIO device. */
#define SDIO_IOCTL_HOST_BUSY		0x1b
#define SDIO_IOCTL_HOST_EVENT_DONE	0x49
#define SDIO_IOCTL_HOST_SLEEPING	0x4a

/* Timer ticks (ms) and times (us). */
#define WLAN_STA_PS_NULL_TICKS		100
#define WLAN_STA_PS_CONFIRM_TICKS	3000
#define WLAN_STA_PS_POLL_TIMEOUT	15000

/* The machine and the station's power-save state. */
struct WlanStaPsMachine wlanStaPs __attribute__((section(".dtcm.bss")));

uint8_t wlanPsBtShareMode __attribute__((section(".itcm.rw")));		/* 0x54aa8 */
uint8_t wlanPsPollFlag __attribute__((section(".itcm.rw")));			/* 0x54aa9 */
uint8_t wlanPsHostWakePending __attribute__((section(".itcm.rw")));		/* 0x54aaa */
uint8_t wlanPsLastTxComplete __attribute__((section(".itcm.rw")));		/* 0x54aab */
uint8_t wlanPsNullSuppressed __attribute__((section(".itcm.rw")));		/* 0x54aac */
uint8_t wlanPsPeriodHold __attribute__((section(".itcm.rw")));		/* 0x54aad */
uint8_t wlanPsConfirmArmed __attribute__((section(".itcm.rw")));		/* 0x54aae */
uint8_t wlanPsSleepAllowance __attribute__((section(".itcm.rw")));		/* 0x54aaf */
uint8_t wlanPsRoleMask __attribute__((section(".itcm.rw")));			/* 0x54ab0 */
uint8_t wlanPsPeerRoleMask __attribute__((section(".itcm.rw")));		/* 0x54ab1 */
uint8_t wlanPsPeriodAnnounced __attribute__((section(".itcm.rw")));		/* 0x54ab2 */
uint16_t wlanPsHoldSeconds __attribute__((section(".itcm.rw"))) = 30;		/* 0x54ab4 */
uint32_t wlanPsLastHostEvent __attribute__((section(".itcm.rw")));		/* 0x54ab8 */
uint32_t wlanPsDataPending __attribute__((section(".itcm.rw")));		/* 0x54abc */
uint32_t wlanPsIeeeFlag __attribute__((section(".itcm.rw")));			/* 0x54ac0 */
uint32_t wlanTxPowerSaveCount __attribute__((section(".itcm.rw")));		/* 0x54ac4 */
uint32_t wlanPsPollSkips __attribute__((section(".itcm.rw")));		/* 0x54ac8 */
uint32_t wlanPsNullRetries __attribute__((section(".itcm.rw")));		/* 0x54acc */
uint32_t wlanPsNullTimeouts __attribute__((section(".itcm.rw")));		/* 0x54ad0 */
uint32_t wlanPsPowerStateChanges __attribute__((section(".itcm.rw")));	/* 0x54ad4 */
uint32_t wlanPsRetries __attribute__((section(".itcm.rw")));			/* 0x54ad8 */
uint32_t wlanPsRetryTimer __attribute__((section(".itcm.rw")));		/* 0x54adc */
uint32_t wlanPsCalibrations __attribute__((section(".itcm.rw")));		/* 0x54ae0 */
uint32_t wlanPsCalibrationDue __attribute__((section(".itcm.rw")));		/* 0x54ae4 */
uint32_t wlanPsPeriodWakes __attribute__((section(".itcm.rw")));		/* 0x54ae8 */
uint32_t wlanPsIeeeFlagAtSleep __attribute__((section(".itcm.rw")));		/* 0x54aec */
uint32_t wlanPsPeriodSleeps __attribute__((section(".itcm.rw")));		/* 0x54af0 */
uint32_t wlanPsPeriodMisses __attribute__((section(".itcm.rw")));		/* 0x54af4 */
uint32_t wlanPsPeriodAborts __attribute__((section(".itcm.rw")));		/* 0x54af8 */
uint32_t wlanPsPeriodDataSleeps __attribute__((section(".itcm.rw")));		/* 0x54afc */
uint32_t wlanPsPeriodSleepTime __attribute__((section(".itcm.rw")));		/* 0x54b00 */
uint32_t wlanPsPeriodEntries __attribute__((section(".itcm.rw")));		/* 0x54b04 */
uint32_t wlanPsHostBusy __attribute__((section(".itcm.rw")));			/* 0x54b08 */
uint32_t wlanPsListenTimer __attribute__((section(".itcm.rw")));		/* 0x54b0c */
uint32_t wlanPsLastEvent __attribute__((section(".itcm.rw")));		/* 0x54b10 */
uint32_t wlanPsHostWakeSkips __attribute__((section(".itcm.rw")));		/* 0x54b14 */
uint32_t wlanPsIeeeEntries __attribute__((section(".itcm.rw")));		/* 0x54b18 */
uint64_t wlanPsPeriodNow __attribute__((section(".itcm.rw")));		/* 0x54b20 */
uint64_t wlanPsPeriodStart __attribute__((section(".itcm.rw")));		/* 0x54b28 */
uint64_t wlanPsPollReplyTime __attribute__((section(".itcm.rw")));		/* 0x54b30 */
uint64_t wlanPsPollSentTime __attribute__((section(".itcm.rw")));		/* 0x54b38 */
uint64_t wlanPsHeaderDeadline __attribute__((section(".itcm.rw")));		/* 0x54b40 */

/* Not converted yet; provisional names. */
extern uint32_t wlanPsWakeAdvance;			/* 0x56260 */
extern struct OsaBufferPool wlanTxFramePool;
extern uint16_t wlanPsHeaderSpacing;			/* 0x562b0 */
extern struct WlanPsHostConfig wlanPsHostConfig;	/* 0x568be */
int wlanPsResetDescriptor(struct WlanConnection *connection, const void *beacon); /* 0x3e98a */
extern volatile uint32_t wlanPsEventCounts[6];		/* 0x56888 */
extern uint8_t wlanSavedMgmtPriority;			/* 0x040088da */
void cbProcAdjustContext(struct WlanConnection *connection);	/* 0x1b5ce */
unsigned int cbProcNullPriority(void);			/* 0x1e9ba */
int p2pDirectReleaseEntry(void);			/* 0x419a6 */
int wlanDirectLinkPoll(struct WlanConnection *connection, uintptr_t *message); /* 0x41050 */
void macMgmtValidateTable(int reason);			/* 0x3726c */
int macTxNotifyApplyState(void);			/* 0x2fefa */
int macTxNotifyBuildState(void);			/* 0x2fbe8 */
int wlanPsProcessHeader(void);				/* 0x3dd26 */
uint32_t wlanPsParseHeader(uint64_t *wake, uint16_t *margin);	/* 0x3defe */
void wlanPsPrepareHeader(uint64_t *next);		/* 0x3dd98 */
void uapSqramFilterEntry(int enable);			/* SQRAM 0xc0009ba0 */

static inline __attribute__((always_inline)) void wlanStaPsRestartTimer(TX_TIMER *timer, ULONG ticks)
{
	tx_timer_deactivate(timer);
	tx_timer_change(timer, ticks, 0);
	tx_timer_activate(timer);
}

static inline __attribute__((always_inline)) void wlanStaPsSendIeeeNull(struct WlanConnection *connection,
								       int powerManagement, int trigger)
{
	macTxSendNullFrame(connection, powerManagement, cbProcNullPriority(), 0, trigger,
			   WLAN_NULL_KIND_IEEE);
}

/* 0x143dc */
void wlanPsArmRetryTimer(uint32_t delay, OsaTimerCallback callback)
{
	osaCancelTimer(wlanPsRetryTimer);
	osaCreateTimer(callback, (uint32_t)&wlanPsRetryTimer, delay, &wlanPsRetryTimer, 0);
}

/* 0x143fe */
void wlanPsCancelRetryTimer(void)
{
	osaCancelTimer(wlanPsRetryTimer);
	wlanPsRetryTimer = 0;
}

/* 0x1440e */
void wlanPsClearHostEvent(void)
{
	wlanPsLastHostEvent = 0;
	mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_EVENT_DONE, 0);
}

/* 0x14424: a null frame with the PM bit; a trigger unless U-APSD is entering power save. */
void wlanStaPsSendNull(struct WlanConnection *connection, int powerManagement)
{
	int trigger;

	macTxSetPowerSave(powerManagement);
	trigger = wlanStaPs.uapsd == 0 || powerManagement != 1;
	macTxSendNullFrame(connection, powerManagement, cbProcNullPriority(), 0, trigger,
			   WLAN_NULL_KIND_STATION);
}

/* 0x1445a: go back to sleep, or to the other mode the host asked for. */
void wlanStaPsRestart(struct WlanStaPsMachine *machine)
{
	struct RomHsmState *target;
	struct WlanConnection *connection;

	if (wlanPsAltModeRequest != 0) {
		wlanStaPsStartPeriodic(machine);
		target = &machine->periodic;
	} else {
		connection = machine->connection;
		sleepCalibrationUpdate();
		sleepWake();
		wlanTxPowerSaveCount = 1;
		wlanPsHostBusy = 0;
		machine->nullRetry = 0;
		machine->powerSaveChanged = romGetTimerTimestamp();
		wlanPsSetPowerState(1);
		wlanPsArmIdleWakeTimer();
		macMgmtValidateTable(3);
		wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
		target = &machine->sleep;
	}
	wlanPsAltModeActive = wlanPsAltModeRequest;
	romHsmTransition(machine, target);
}

/* 0x144cc */
void wlanPsListenTimerExpired(uint32_t timer, uint32_t argument)
{
	wlanRaiseEvent(WLAN_STA_PS_EVENT_LISTEN);
}

/* 0x144d6: nothing keeps the station awake. */
int wlanStaPsIdle(void)
{
	return macTxPoolsIdle() && wlanHostQueueDepth(0) == 0 && wlanHostQueueDepth(0) == 0 &&
	       wlanSlotHasCurrent() == 0 && wlanPsIdle() && wlanStaPs.btShare != 2 &&
	       mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PENDING, 0) == 0 && !p2pDirectReleaseEntry();
}

/* 0x1452c: something keeps the station from sleeping. */
int wlanStaPsBusy(void)
{
	int busy;

	if ((wlanPsListenBeacons != 0 && wlanStaPs.dataPendingAtConfirm != wlanPsDataPending) ||
	    wlanStaPs.beaconWait != 0 || mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_BUSY, 0) != 0 ||
	    mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PENDING, 0) != 0 || wlanSlotHasCurrent() != 0 ||
	    wlanHostQueueDepth(0) != 0 || wlanHostQueueDepth(0) != 0)
		busy = 1;
	else
		busy = p2pDirectReleaseEntry();
	if (busy != 0 && busy != 0)
		busy = 1;
	if (busy == 0 && wlanStaPs.servicePeriod == 0)
		return !macTxPoolsIdle();
	return busy;
}

/* 0x145a6: the PM bit has not changed for wlanPsHoldSeconds. */
int wlanStaPsHoldExpired(void)
{
	uint64_t elapsed;

	if (wlanPsHoldSeconds >= 0xffff || wlanStaPs.powerSaveChanged == 0)
		return 0;
	elapsed = (romGetTimerTimestamp() - wlanStaPs.powerSaveChanged) / 1000;
	return elapsed / 1000 >= wlanPsHoldSeconds;
}

/* 0x145f2: sleep until the next beacon or timer; 1 when a timer is too close. */
int wlanStaPsDeepSleep(void)
{
	uint64_t wake;
	uint64_t timerDeadline;
	uint16_t margin;
	uint32_t duration;
	uint32_t next;

	if (wlanPsProcessHeader() != 0)
		return 0;
	duration = wlanPsParseHeader(&wake, &margin);
	next = osaGetNextTimerDeadline(&timerDeadline, 2, 1);
	if (next != 0 && next < duration) {
		wlanPsSetPowerState(4);
		wake = timerDeadline;
	} else {
		wlanPsSetPowerState(5);
		next = duration;
	}
	if (osaExpireTimerWithin(next + margin) != 0) {
		wlanPsSetPowerState(4);
		return 1;
	}
	return sleepEnter(&wake, margin, 1000, next, (uint8_t)wlanStaPs.servicePeriod);
}

/* 0x1466c */
const int32_t *wlanStaPsRoot(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection;
	unsigned int tries;

	wlanPsLastEvent = *event;
	connection = machine->connection;
	switch (*event) {
	case 47:
		wlanPsNullTimeouts++;
		/* fall through */
	case 40:
		if (machine->ieeeState != 2 && machine->ieeeState != 1)
			break;
		tries = machine->ieeeTries++;
		if (tries >= 2)
			osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
		else
			wlanStaPsSendIeeeNull(connection, machine->ieeeState == 1, 0);
		break;
	case WLAN_STA_PS_EVENT_IEEE_ENTER:
		for (;;)
			wlanPsEventCounts[0]++;
	case WLAN_STA_PS_EVENT_IEEE_EXIT:
		for (;;)
			wlanPsEventCounts[1]++;
	case 43:
	case 44:
	case 45:
		for (;;)
			wlanPsEventCounts[2]++;
	case WLAN_STA_PS_EVENT_START:
		for (;;)
			wlanPsEventCounts[4]++;
	case WLAN_STA_PS_EVENT_STOP:
		for (;;)
			wlanPsEventCounts[5]++;
	case 3:
		machine->powerStateDue = 1;
		wlanPsSetPowerState(2);
		wlanPsPowerStateChanges++;
		break;
	case WLAN_STA_PS_EVENT_WAKE:
		machine->eventPending = 1;
		break;
	case ROM_HSM_ENTRY:
		romHsmTransition(machine, &machine->active);
		break;
	}
	return NULL;
}

/* 0x14748 */
const int32_t *wlanStaPsIeee(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;

	switch (*event) {
	case WLAN_STA_PS_EVENT_START:
		machine->restartDue = 1;
		machine->resume = &machine->sleep;
		cbProcAdjustContext(connection);
		return NULL;
	case WLAN_STA_PS_EVENT_STOP:
		machine->resume = &machine->active;
		cbProcAdjustContext(connection);
		return NULL;
	case WLAN_STA_PS_EVENT_RESTART:
		machine->restartDue = 1;
		return NULL;
	case ROM_HSM_EXIT:
		wlanPsSetIeeeActive(0);
		return NULL;
	case ROM_HSM_ENTRY:
		wlanPsSetIeeeActive(1);
		return NULL;
	}
	return event;
}

/* 0x14798: back to PM=0 with the access point. */
void wlanStaPsWakeAp(struct WlanStaPsMachine *machine)
{
	struct WlanConnection *connection = machine->connection;

	sleepCalibrationUpdate();
	sleepWake();
	if (macTxGetPowerSave() == 1) {
		wlanStaPsSendNull(connection, 0);
		wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
	}
}

/* 0x147f4 */
const int32_t *wlanStaPsIeeeExit(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;

	switch (*event) {
	case 43:
		if (machine->resume == &machine->active) {
			wlanStaPsWakeAp(machine);
		} else {
			if (macTxGetPowerSave() == 0) {
				machine->ieeeTries = 0;
				machine->ieeeState = 1;
				macTxSetPowerSave(1);
				wlanStaPsSendIeeeNull(connection, 1, 0);
			}
			if (machine->exitEventDue)
				wlanRaiseEvent(9);
		}
		romHsmTransition(machine, machine->resume);
		return NULL;
	case 44:
	case 45:
		tx_timer_deactivate(&machine->nullTimer);
		if (machine->resume != &machine->active)
			wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
		machine->resume = &machine->active;
		machine->ieeeState = 0;
		macTxSetPowerSave(0);
		sleepCalibrationUpdate();
		sleepWake();
		machine->eventPending = 0;
		romHsmTransition(machine, machine->resume);
		return NULL;
	case ROM_HSM_ENTRY:
		machine->ieeeTries = 0;
		/* fall through */
	case ROM_HSM_EXIT:
		osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
		return NULL;
	}
	return event;
}

/* 0x148c6: PM=1 null frames until the access point acknowledges, at most three more. */
const int32_t *wlanStaPsIeeeNull(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	unsigned int tries;

	switch (*event) {
	case 32:
	case 33:
		wlanPsPollHandleEvent(connection, *event);
		return NULL;
	case 39:
		if (machine->ieeeState == 1)
			goto gaveUp;
		return NULL;
	case ROM_HSM_EXIT:
		goto done;
	case ROM_HSM_ENTRY:
		machine->ieeeState = 0;
		machine->ieeeTries = 0;
		machine->wakeBits = macTxAcquireWakeBit();
		if (!(connection->flags2 & 0x10))
			goto done;
		if (connection->mode != 3 && connection->mode != 1)
			goto done;
		if (macTxGetPowerSave() != 0)
			goto done;
		macTxSetPowerSave(1);
		if ((wlanTxPowerSaveCount | wlanPsIeeeFlag) == 0)
			goto done;
		machine->ieeeState = 1;
		wlanStaPsSendIeeeNull(connection, 1, 0);
		return NULL;
	case 43:
		wlanPsIeeeFlag = 0;
		wlanTxPowerSaveCount = 0;
		if (machine->resume == &machine->active && machine->ieeeState == 3) {
			macTxSetPowerSave(0);
			machine->ieeeTries = 0;
			machine->ieeeState = 2;
			wlanStaPsSendIeeeNull(connection, 0, 1);
		}
		macTxReleaseWakeBits(&machine->wakeBits);
		wlanPsSetPowerState(1);
		wlanPsArmIdleWakeTimer();
		romHsmTransition(machine, machine->resume);
		return NULL;
	case 44:
	case 45:
		tx_timer_deactivate(&machine->nullTimer);
		wlanPsSetPowerState(1);
		wlanPsArmIdleWakeTimer();
		machine->resume = &machine->active;
		machine->ieeeState = 0;
		macTxSetPowerSave(0);
		sleepCalibrationUpdate();
		sleepWake();
		macTxReleaseWakeBits(&machine->wakeBits);
		machine->eventPending = 0;
		romHsmTransition(machine, machine->resume);
		return NULL;
	case 47:
		wlanPsNullRetries++;
		/* fall through */
	case 40:
		if (machine->ieeeState == 1) {
			tries = machine->ieeeTries++;
			if (tries > 2)
				goto gaveUp;
			wlanStaPsSendIeeeNull(connection, 1, 0);
		} else if (machine->ieeeState == 2) {
			tries = machine->ieeeTries++;
			if (tries > 2)
				goto gaveUp;
			wlanStaPsSendIeeeNull(connection, 0, 0);
		}
		return NULL;
	default:
		return event;
	}

gaveUp:
	machine->ieeeState = 3;
done:
	osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
	return NULL;
}

/* 0x14a5e */
int wlanCoexUseStationMode(void)
{
	return wlanRoleOtherOnly() && wlanPsAltModeActive != 0 && (wlanPsRoleMask & 0x0f) == 0 &&
	       wlanPsNullSuppressed == 0;
}

/* 0x14a8a */
int wlanCoexUseP2pMode(void)
{
	if ((wlanWmmEnabled() && (wlanPsRoleMask & 0x0f) != 0) || wlanPsNullSuppressed != 0)
		return wlanPsAltModeActive != 0;
	return 0;
}

/* 0x14ab6: power save is off. */
const int32_t *wlanStaPsActive(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;

	switch (*event) {
	case 15:
		machine->nullRetry = 1;
		/* fall through */
	case 16:
		if ((connection->flags2 & 0x10) && machine->nullRetry == 0) {
			wlanStaPsSendNull(connection, 0);
			return NULL;
		}
		/* fall through */
	case 13:
		tx_timer_deactivate(&machine->nullTimer);
		/* fall through */
	case WLAN_STA_PS_EVENT_STOP:
		cbProcAdjustContext(connection);
		return NULL;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		macTxWakeup();
		wlanPsSetOverride(1);
		machine->nullRetry = 0;
		machine->powerSaveChanged = 0;
		macTxSetPowerSave(0);
		machine->directLink = 1;
		if (machine->slotHeld) {
			machine->slotHeld = 0;
			wlanSlotResume();
		}
		if (machine->eventPending) {
			wlanRaiseEvent(WLAN_STA_PS_EVENT_WAKE);
			machine->eventPending = 0;
		}
		macMgmtValidateTable(6);
		wlanPsEvaluate(connection, 1);
		return NULL;
	case WLAN_STA_PS_EVENT_START:
		cbProcAdjustContext(connection);
		if (wlanCoexUseP2pMode() || wlanCoexUseStationMode()) {
			wlanStaPsBeginPeriodic(machine);
			macMgmtValidateTable(4);
			romHsmTransition(machine, &machine->periodic);
			return NULL;
		}
		wlanTxPowerSaveCount = 1;
		wlanPsSetPowerState(2);
		macMgmtValidateTable(3);
		machine->nullRetry = 0;
		machine->powerSaveChanged = romGetTimerTimestamp();
		if (wlanCoexBtNeedsSharing() || wlanPsBtShareMode == 1) {
			wlanStaPsSendNull(connection, 1);
			wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
		}
		machine->btShare = wlanPsBtShareMode;
		machine->uapsd = 0;
		machine->servicePeriod = 0;
		wlanPsEvaluate(connection, 0);
		romHsmTransition(machine, &machine->sleep);
		return NULL;
	case WLAN_STA_PS_EVENT_WAKE:
		wlanPostConnectionEvent(connection, WLAN_EVENT_HOST_PS_PEER);
		return NULL;
	case 32:
	case 33:
		wlanPsPollHandleEvent(connection, *event);
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_ENTER:
		machine->resume = &machine->active;
		romHsmTransition(machine, &machine->ieeeNull);
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_EXIT:
		machine->resume = &machine->active;
		romHsmTransition(machine, &machine->ieeeExit);
		return NULL;
	case WLAN_STA_PS_EVENT_RESTART:
		wlanPsAltModeActive = wlanPsAltModeRequest;
		return NULL;
	}
	return event;
}

/* 0x14c70: power save is on. */
const int32_t *wlanStaPsPowerSave(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;

	switch (*event) {
	case WLAN_STA_PS_EVENT_STOP:
		sleepCalibrationUpdate();
		sleepWake();
		if (macTxGetPowerSave() == 1 || wlanTxPowerSaveCount == 0) {
			wlanStaPsSendNull(connection, 0);
			wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
		} else {
			cbProcAdjustContext(connection);
		}
		romHsmTransition(machine, &machine->active);
		return NULL;
	case WLAN_STA_PS_EVENT_WAKE:
		sleepCalibrationUpdate();
		sleepWake();
		macTxSetPowerSave(0);
		tx_timer_deactivate(&machine->nullTimer);
		tx_timer_deactivate(&machine->confirmTimer);
		wlanPsRetries = 0;
		wlanPsCancelRetryTimer();
		cbProcAdjustContext(connection);
		wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
		wlanPostConnectionEvent(connection, WLAN_EVENT_HOST_PS_PEER);
		wlanCoexHandleEvent(connection, 17);
		romHsmTransition(machine, &machine->active);
		return NULL;
	case 38:
		wlanPsEvaluate(connection, 3);
		return NULL;
	case WLAN_STA_PS_EVENT_RESTART:
		machine->restartDue = 1;
		return NULL;
	case ROM_HSM_ENTRY:
		wlanPsSetSleepLeft(0);
		wlanPsSetOverride(0);
		if (machine->eventPending) {
			wlanRaiseEvent(WLAN_STA_PS_EVENT_WAKE);
			machine->eventPending = 0;
		}
		wlanPsSetPowerState(0);
		return NULL;
	case ROM_HSM_EXIT:
		wlanPsSetSleepLeft(1);
		return NULL;
	}
	return event;
}

/* 0x14d7c: the PM bit is set; sleep as soon as nothing keeps the station awake. */
const int32_t *wlanStaPsEnter(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	uint8_t previous[4];

	switch (*event) {
	case 11:
	case 12:
	case 14:
	case 15:
	case 20:
	case 25:
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		wlanTxPowerSaveCount = 0;
		wlanPsDataPending = 0;
		wlanPsIeeeFlag = 0;
		wlanCoexHandleEvent(connection, 51);
		if (machine->btShare == 0)
			macTxSetPowerSave(1);
		sleepCalibrationUpdate();
		machine->beaconsSeen = 0;
		if (machine->powerStateDue) {
			wlanPsSetPowerState(2);
		} else {
			if (machine->uapsd != 0 && machine->servicePeriod != 0) {
				wlanStaPsBusy();
				return NULL;
			}
			if (wlanStaPsDeepSleep() != 0 || (wlanPendingEvents[0] & 8) != 0)
				return NULL;
			wlanPsSetPowerState(1);
			wlanPsArmIdleWakeTimer();
		}
		romHsmTransition(machine, &machine->sleep);
		return NULL;
	case 3:
		if (wlanSavedMgmtPriority != 0xff) {
			osaTaskSetPriority(macMgmtTask, wlanSavedMgmtPriority, previous);
			wlanSavedMgmtPriority = 0xff;
		}
		machine->directLink = wlanDirectLinkPoll(connection, NULL) != 0;
		if (machine->hostState == 4) {
			wlanPsHostWakeSkips++;
			return NULL;
		}
		romHsmTransition(machine, &machine->sleep);
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_ENTER:
		wlanPsIeeeEntries++;
		machine->resume = &machine->sleep;
		romHsmTransition(machine, &machine->ieeeNull);
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_EXIT:
		machine->resume = &machine->sleep;
		romHsmTransition(machine, &machine->ieeeExit);
		return NULL;
	}
	return event;
}

/* 0x14ea2 */
int wlanPsLegacyPollMode(void)
{
	if (!wlanRoleOtherOnly() || (wlanPsRoleMask & 0x0f) != 0 || wlanPsNullSuppressed != 0)
		return 0;
	if (wlanPsAltModeActive != 0)
		return 1;
	return wlanStaPs.btShare == 1 && !wlanPsDisabled();
}

/* 0x14ee0: poll the access point for its buffered frames. */
void wlanPsEnterSleep(struct WlanConnection *connection, int urgent)
{
	int sent;

	if (wlanPsBtHoldWanted()) {
		wlanPsSetBtHold(1);
		if (!wlanPsPollUsesMacTimer()) {
			wlanPsPollStartRetryTimer();
			return;
		}
		if (wlanPsPollReserve() != 0)
			return;
		sent = wlanPsPollSend(connection);
	} else {
		sent = macTxSendPsPoll(connection, urgent);
	}
	if (sent == 1)
		wlanPsPollSentTime = romGetTimerTimestamp();
}

/* 0x14f2c: a U-APSD trigger: a PM=1 null frame. */
void wlanPsSendPowerSaveNull(struct WlanConnection *connection)
{
	if (wlanPsNullSuppressed != 0)
		return;
	macTxSendNullFrame(connection, 1, cbProcNullPriority(), 1, 1, WLAN_NULL_KIND_TRIGGER);
}

/* 0x14f50 */
int wlanCoexUseP2pSleepMode(void)
{
	if ((wlanWmmEnabled() && (wlanPsRoleMask & 0x0f) != 0) || wlanPsNullSuppressed != 0) {
		if (wlanPsAltModeActive != 0)
			return 1;
		return wlanStaPs.btShare == 1 && !wlanPsDisabled();
	}
	return 0;
}

/* 0x14f8e */
int wlanStaPsNotifyPending(void)
{
	int pending = macTxNotifyApplyState();

	if (pending != 0)
		pending = macTxNotifyBuildState();
	return pending;
}

/* 0x14f9e: the station sleeps between beacons. */
const int32_t *wlanStaPsSleep(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	uintptr_t message;
	uint32_t post;
	uint64_t now;
	int pm;

	switch (*event) {
	case ROM_HSM_ENTRY:
		macTxWakeup();
		if (machine->powerStateDue) {
			wlanPsSetPowerState(2);
			machine->powerStateDue = 0;
		}
		if (machine->eventPending) {
			wlanRaiseEvent(WLAN_STA_PS_EVENT_WAKE);
			machine->eventPending = 0;
		}
		if (machine->restartDue) {
			machine->restartDue = 0;
			wlanStaPsRestart(machine);
			if (wlanPsAltModeActive != 0)
				return NULL;
		}
		sleepCalibrationKick(1);
		machine->confirmBits = 0;
		machine->dataPendingSeen = 0;
		machine->ieeeFlagSeen = 0;
		wlanPsConfirmArmed = 0;
		wlanPsLastTxComplete = 1;
		wlanPsPollReplyTime = 0;
		wlanPsPollSentTime = 0;
		wlanMefConfig.sleepHeld = 0;
		wlanPsSleepTimeout = 0;
		wlanPsSleepSequence = 0;
		wlanPsHostWakeDue = 0;
		post = 1;
		if (machine->hostState == 2 || mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_BUSY, 0) != 0) {
			machine->hostState = 0;
			wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
		}
		if (wlanStaPsNotifyPending())
			post = 9;
		if (wlanSubscribeEventsPending(connection))
			wlanPostConnectionEvent(connection, 0x08000000);
		if (wlanPsMgmtWakeDue == 1) {
			wlanPsMgmtWakeDue = 0;
			osaEventFlagsSet(macMgmtEventFlags, 2, 7);
		}
		if (wlanCoexBtNeedsSharing() || wlanPsBtShareMode == 1) {
			machine->btShare = 1;
			wlanPsEvaluate(connection, 2);
			if (wlanPsBtShareMode != 1) {
				machine->btShare = wlanPsBtShareAllowed();
				if (machine->btShare == 0)
					machine->uapsd = 1;
			}
			if (wlanStaPsHoldExpired() && wlanCoexBtNeedsSharing() && machine->btShare == 1) {
				if (wlanPsTriggerWanted()) {
					machine->triggerSent = 1;
					macTxSendNullFrame(connection, 1, cbProcNullPriority(), 0, 1,
							   WLAN_NULL_KIND_TRIGGER);
				} else {
					wlanCoexSetHold();
				}
			}
			if (machine->btShare != 0)
				goto entered;
			if (machine->uapsd == 0 || machine->servicePeriod == 0)
				goto awake;
			if (!wlanPsTriggerWanted()) {
				wlanCoexSetHold();
				goto entered;
			}
			if (!macTxPoolsIdle())
				goto entered;
			wlanStaPsSendNull(connection, 0);
			wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
			goto entered;
		}
		if (machine->btShare == 1 || machine->uapsd != 0)
			wlanPsEvaluate(connection, 2);
		machine->btShare = 0;
		machine->uapsd = 0;
awake:
		macTxSetPowerSave(0);
entered:
		machine->servicePeriod = 0;
		macTxPostEvent(post);
		if (machine->beaconsSeen)
			wlanPsArmWakeTimer();
		if (machine->slotHeld) {
			machine->slotHeld = 0;
			wlanSlotResume();
		}
		return NULL;
	case ROM_HSM_EXIT:
		wlanPsCancelWakeTimer();
		wlanPsPollStop();
		if (macTxGetPowerSave() == 1 && (wlanPsIeeeFlag | wlanPsDataPending) != 0)
			machine->powerSaveChanged = romGetTimerTimestamp();
		return NULL;
	case 3:
		if (machine->hostState == 2) {
			machine->hostState = 0;
			wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
			macTxPostEvent(1);
			wlanPsHostWakeDue = 0;
		}
		return NULL;
	case 11:
		if (wlanMefConfig.hostState != 0)
			return NULL;
		if (machine->beaconWait != 0 &&
		    (!wlanHostSleepArmed() || wlanMefConfig.queuedEvents != 0)) {
			if (machine->btShare == 1) {
				if (macTxGetPowerSave() == 1) {
					if (wlanCoexUseP2pSleepMode()) {
						if (wlanTxRingsPending())
							wlanPsTriggerSent();
						else if (machine->ieeeFlagSeen == wlanPsIeeeFlag)
							wlanPsSendPowerSaveNull(connection);
					} else {
						wlanPsEnterSleep(connection, 0);
					}
					wlanPsLastTxComplete = 0;
					wlanCoexHandleEvent(connection, 11);
				}
			} else if (wlanPsIeeeFlag == 0) {
				if (machine->uapsd != 0) {
					if (wlanPsTriggerWanted())
						goto resendNull;
				} else {
					message = 0;
					if (wlanDirectLinkPoll(connection, &message) == 1) {
resendNull:
						wlanStaPsSendNull(connection, 0);
						wlanStaPsRestartTimer(&machine->nullTimer,
								      WLAN_STA_PS_NULL_TICKS);
					} else {
						machine->beaconsSeen |= 1;
					}
				}
			}
		}
		macTxPostEvent(5);
		machine->dataPendingSeen = wlanPsDataPending;
		machine->ieeeFlagSeen = wlanPsIeeeFlag;
		if (machine->beaconWait != 0)
			wlanPsArmWakeTimer();
		else
			wlanPsRearmWakeTimer();
		return NULL;
	case 12:
		if (macTxGetPowerSave() == 1)
			wlanPsLastTxComplete = 1;
		if (wlanMefConfig.hostState != 0)
			return NULL;
		macTxPostEvent(5);
		machine->dataPendingSeen = wlanPsDataPending;
		machine->ieeeFlagSeen = wlanPsIeeeFlag;
		if (wlanPsWakeTimerRunning() == 0 && wlanStaPsIdle() != 0)
			goto confirm;
		wlanPsTriggerSent();
		return NULL;
	case 13:
		if (macTxGetPowerSave() == 1)
			return NULL;
		if (wlanMefConfig.queuedEvents != 0)
			macTxPostEvent(1);
		machine->beaconsSeen |= 1;
		tx_timer_deactivate(&machine->nullTimer);
		return NULL;
	case 14:
		tx_timer_deactivate(&machine->nullTimer);
		machine->powerSaveChanged = romGetTimerTimestamp();
		if (machine->btShare != 2)
			return NULL;
		machine->btShare = 1;
		wlanPsEvaluate(connection, 2);
		machine->btShare = wlanPsBtShareAllowed();
		if (machine->btShare != 0)
			return NULL;
		machine->uapsd = 1;
		macTxSetPowerSave(0);
		return NULL;
	case 15:
	case 16:
		if (machine->btShare == 2 || machine->btShare == 1) {
			if (*event != 16 && !macTxPoolsIdle())
				goto restartNull;
			pm = 1;
		} else {
			if (!macTxPoolsIdle())
				goto restartNull;
			pm = 0;
		}
		wlanStaPsSendNull(connection, pm);
restartNull:
		wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
		return NULL;
	case 19:
		wlanPsArmWakeTimer();
		macTxPostEvent(1);
		return NULL;
	case 21:
		if (wlanPsIeeeFlag == 0 || wlanPsDataPending == 0 || wlanPsConfirmArmed != 1)
			goto triggerSent;
		if (wlanStaPsIdle() == 0 || wlanDmaRingsHaveRoom() == 1)
			goto triggerSent;
		goto confirm;
	case 25:
		wlanPsConfirmArmed = 1;
		return NULL;
	case 26:
		if (wlanPsDataPending == 0 || wlanDmaRingsHaveRoom() != 0)
			return NULL;
		wlanPsEvaluate(connection, 4);
		if (machine->ieeeFlagSeen != wlanPsIeeeFlag || machine->beaconsSeen > 1 ||
		    machine->beaconWait != 0)
			return NULL;
		wlanPsCancelWakeTimer();
		/* fall through */
	case 20:
		if (wlanPsWakeTimerRunning() != 0)
			return NULL;
		if (macTxGetPowerSave() == 1) {
			if (wlanPsLastTxComplete == 0 || wlanCoexDeferred() != 0) {
				wlanPsArmWakeTimer();
				return NULL;
			}
			if (wlanPsLegacyPollMode() && wlanPsPollSentTime != 0 &&
			    wlanPsPollReplyTime < wlanPsPollSentTime) {
				now = romGetTimerTimestamp();
				if (now < wlanPsPollSentTime + WLAN_STA_PS_POLL_TIMEOUT) {
					wlanPsArmWakeTimerIn(wlanPsPollSentTime - now + WLAN_STA_PS_POLL_TIMEOUT);
					return NULL;
				}
				wlanPsPollStop();
			}
		}
		if (wlanStaPsIdle() && wlanMefConfig.hostState == 0 && wlanMefConfig.hostEvents == 0)
			goto confirm;
		wlanPsArmWakeTimer();
triggerSent:
		wlanPsTriggerSent();
		return NULL;
	case 27:
		tx_timer_deactivate(&machine->nullTimer);
		goto confirm;
	case 28:
		machine->hostState = 0;
		wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
		return NULL;
	case 32:
	case 33:
		wlanPsPollHandleEvent(connection, *event);
		return NULL;
	case 34:
		if (machine->uapsd == 0 || wlanPsCoexWindowEnd() == 0)
			return NULL;
		tx_timer_deactivate(&machine->nullTimer);
		machine->servicePeriod = 1;
		goto confirm;
	case 35:
		if (wlanPsBtShareMode == 1) {
			if (machine->btShare == 1)
				return NULL;
			for (;;)
				;
		}
		if (machine->btShare == 1) {
			machine->btShare = 0;
			macTxSetPowerSave(0);
			if (wlanPsIeeeFlag != 0)
				machine->powerSaveChanged = romGetTimerTimestamp();
			machine->ieeeFlagSeen = 0;
			wlanPsIeeeFlag = 0;
		} else if (machine->btShare == 0) {
			machine->uapsd = 0;
		}
		return NULL;
	case 36:
		if (wlanPsBtShareMode == 1) {
			if (machine->btShare != 1)
				for (;;)
					;
			wlanPsEvaluate(connection, 2);
			return NULL;
		}
		if (machine->btShare != 0 || !wlanCoexBtNeedsSharing())
			return NULL;
		machine->btShare = 2;
		wlanStaPsSendNull(connection, 1);
		wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
		return NULL;
	case 38:
		wlanPsEvaluate(connection, 3);
		if (!wlanCoexHold())
			return NULL;
		wlanCoexClearHold();
		if (wlanStaPsHoldExpired() && wlanCoexBtNeedsSharing() && machine->btShare == 1 &&
		    wlanPsTriggerWanted()) {
			machine->triggerSent = 1;
			macTxSendNullFrame(connection, 1, cbProcNullPriority(), 0, 1,
					   WLAN_NULL_KIND_TRIGGER);
			return NULL;
		}
		if (machine->btShare != 0 || machine->uapsd == 0 || !wlanPsTriggerWanted() ||
		    !macTxPoolsIdle())
			return NULL;
		wlanStaPsSendNull(connection, 0);
		wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_ENTER:
		machine->resume = &machine->sleep;
		romHsmTransition(machine, &machine->ieeeNull);
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_EXIT:
		machine->resume = &machine->sleep;
		romHsmTransition(machine, &machine->ieeeExit);
		return NULL;
	case WLAN_STA_PS_EVENT_RESTART:
		machine->restartDue = 0;
		wlanStaPsRestart(machine);
		return NULL;
	default:
		return event;
	}

confirm:
	romHsmTransition(machine, &machine->confirm);
	return NULL;
}

/* 0x15690: the PM=1 null frame and the host's confirmation, then Sleep or Enter. */
const int32_t *wlanStaPsConfirm(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	uint32_t irq;
	int hostSleep = 0;
	int wasPowerSave;
	uint16_t bits;

	switch (*event) {
	case 11:
	case 12:
	case 20:
	case 21:
	case 25:
	case 32:
	case 33:
		return NULL;
	case 31:
		if (wlanStaPsBusy() != 0)
			goto asleep;
		wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_SLEEP, 0);
		tx_timer_deactivate(&machine->confirmTimer);
		tx_timer_change(&machine->confirmTimer, WLAN_STA_PS_CONFIRM_TICKS, 0);
		tx_timer_activate(&machine->confirmTimer);
		return NULL;
	case 37:
		irq = sysIrqDisable();
		goto decide;
	case WLAN_STA_PS_EVENT_IEEE_ENTER:
		machine->resume = &machine->sleep;
		romHsmTransition(machine, &machine->ieeeNull);
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_EXIT:
		machine->resume = &machine->sleep;
		romHsmTransition(machine, &machine->ieeeExit);
		return NULL;
	case WLAN_STA_PS_EVENT_RESTART:
		machine->restartDue = 1;
		goto sleep;
	case ROM_HSM_EXIT:
		tx_timer_deactivate(&machine->confirmTimer);
		osaCancelTimer(wlanPsListenTimer);
		return NULL;
	case ROM_HSM_ENTRY:
		machine->triggerSent = 0;
		machine->nullRetry = 0;
		machine->dataPendingAtConfirm = wlanPsDataPending;
		machine->confirmBits = 0;
		if (wlanPsLastHostEvent == WLAN_EVENT_HOST_PS_SLEEP)
			wlanPsLastHostEvent = 0;
		wlanCoexHandleEvent(connection, 50);
		wasPowerSave = macTxGetPowerSave();
		macTxSetPowerSave(1);
		if ((wlanPsIeeeFlag | wlanTxPowerSaveCount) != 0 || wlanCoexDeferHostSleep() != 0) {
			if (wasPowerSave != 0)
				goto nullDone;
		} else {
			if (!wlanStaPsHoldExpired() ||
			    (wlanCoexBtNeedsSharing() && machine->btShare == 1) ||
			    machine->directLink != 1)
				goto nullDone;
			machine->triggerSent = machine->directLink;
		}
		wlanStaPsSendNull(connection, 1);
		wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
		goto told;
nullDone:
		machine->confirmBits |= WLAN_STA_PS_NULL_DONE;
told:
		if (wlanHostSleepArmed()) {
			if (machine->servicePeriod == 0)
				hostSleep = 1;
			goto hostDone;
		}
		if (machine->servicePeriod != 0)
			goto hostDone;
		if (machine->hostState == 2)
			goto sleep;
		if (mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_SLEEPING, 0) == 0 &&
		    wlanPsLastHostEvent != WLAN_EVENT_HOST_PS_AWAKE)
			goto hostDone;
		if ((machine->confirmBits & WLAN_STA_PS_NULL_DONE) || wlanPsListenBeacons == 0) {
			wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_SLEEP, 0);
			wlanStaPsRestartTimer(&machine->confirmTimer, WLAN_STA_PS_CONFIRM_TICKS);
		}
		goto check;
hostDone:
		machine->confirmBits |= WLAN_STA_PS_HOST_DONE;
check:
		if (machine->confirmBits != (WLAN_STA_PS_NULL_DONE | WLAN_STA_PS_HOST_DONE))
			return NULL;
		irq = sysIrqDisable();
		if (machine->uapsd != 0)
			goto enter;
		if (hostSleep) {
			if (macTxPoolsIdle() != 0)
				goto enter;
			sysIrqRestore(irq);
			goto asleep;
		}
decide:
		if (wlanStaPsBusy() == 0)
			goto enter;
		sysIrqRestore(irq);
		goto asleep;
	case 3:
		if (machine->hostState == 2 ||
		    (machine->uapsd != 0 && (machine->confirmBits & WLAN_STA_PS_HOST_DONE))) {
			wlanTxPowerSaveCount = 0;
			if ((wlanPsDataPending | wlanPsIeeeFlag) != 0)
				machine->powerSaveChanged = romGetTimerTimestamp();
			wlanPsDataPending = 0;
			wlanPsIeeeFlag = 0;
			tx_timer_deactivate(&machine->nullTimer);
			goto sleep;
		}
		return event;
	case WLAN_STA_PS_EVENT_HOST_CONFIRMED:
		tx_timer_deactivate(&machine->confirmTimer);
		machine->confirmBits |= WLAN_STA_PS_HOST_DONE;
		if (machine->confirmBits != (WLAN_STA_PS_NULL_DONE | WLAN_STA_PS_HOST_DONE))
			return NULL;
		goto ready;
	case 10:
		if (wlanMefConfig.wakeSignalled) {
			wlanMefConfig.activated = 1;
			wlanHostEventNotify(0xff, NULL);
		}
		if (machine->btShare == 0) {
			wlanStaPsSendNull(connection, 0);
			wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
		}
		goto asleep;
	case 14:
		goto nullAcked;
	case 15:
		if (machine->uapsd != 0 || macTxPoolsIdle() != 0) {
			machine->nullRetry = 1;
			wlanStaPsSendNull(connection, 1);
		}
		wlanStaPsRestartTimer(&machine->nullTimer, WLAN_STA_PS_NULL_TICKS);
		return NULL;
	case 16:
		if (machine->servicePeriod == 0 && wlanStaPsBusy() != 0) {
			tx_timer_deactivate(&machine->nullTimer);
			goto asleep;
		}
		if (machine->nullRetry != 0)
			goto nullAcked;
		wlanStaPsSendNull(connection, 1);
		return NULL;
	default:
		return event;
	}

nullAcked:
	tx_timer_deactivate(&machine->nullTimer);
	machine->powerSaveChanged = romGetTimerTimestamp();
	bits = machine->confirmBits;
	if (bits & WLAN_STA_PS_NULL_DONE)
		return NULL;
	bits |= WLAN_STA_PS_NULL_DONE;
	machine->confirmBits = bits;
	if (bits != (WLAN_STA_PS_NULL_DONE | WLAN_STA_PS_HOST_DONE)) {
		if (wlanPsListenBeacons != 0)
			osaCreateTimer(wlanPsListenTimerExpired, 0,
				       wlanPsListenBeacons * wlanPsBeaconInterval, &wlanPsListenTimer, 0);
		return NULL;
	}
	if (machine->servicePeriod != 0) {
		wlanCoexStartGuardTimer();
		return NULL;
	}
ready:
	irq = sysIrqDisable();
	if (wlanStaPsBusy() != 0) {
		sysIrqRestore(irq);
		wlanTxPowerSaveCount = 0;
		goto asleep;
	}
enter:
	romHsmTransition(machine, &machine->enter);
	sysIrqRestore(irq);
	return NULL;
asleep:
	wlanPsSetPowerState(5);
sleep:
	romHsmTransition(machine, &machine->sleep);
	return NULL;
}

/* 0x15a5c: the host is to be told before the station sleeps. */
int wlanStaPsShouldNotify(void)
{
	int notify = wlanStaPsNotifyPending();

	if (notify == 0 && wlanPsEnabled() && wlanStaPsHoldExpired())
		notify = 1;
	return notify;
}

/* 0x15a7c: U-APSD is on for some access categories, not all four. */
int wlanPsUapsdPartial(void)
{
	return wlanCoexUseP2pMode() == 1 && (~wlanPsRoleMask & 0x0f) != 0;
}

/* 0x15a9a: an access category U-APSD does not deliver. */
int wlanPsAcLegacy(unsigned int ac)
{
	if (wlanPsUapsdPartial() != 1)
		return 0;
	return ((1u << wlanTspecUapsdBit(ac)) & wlanPsRoleMask) == 0;
}

/* 0x15ac2 */
void wlanPsSetRoleMasks(uint8_t mask, uint8_t peerMask)
{
	wlanPsRoleMask = mask;
	wlanPsPeerRoleMask = peerMask;
}

/* 0x15ad2 */
void wlanPsPeriodTimerExpired(uint32_t timer, uint32_t argument)
{
	if (wlanPsRetryTimer != timer)
		return;
	wlanPsRetryTimer = 0;
	wlanRaiseEvent(22);
}

/* 0x15ae8 */
void wlanPsPeriodRetryExpired(uint32_t timer, uint32_t argument)
{
	if (wlanPsRetryTimer != timer)
		return;
	wlanPsRetries = 0;
	wlanPsRetryTimer = 0;
	uapSqramFilterEntry(2);
	wlanRaiseEvent(3);
}

/* 0x15b06: the next multiple of period (TU) after now (us). */
uint64_t wlanPsNextPeriod(uint64_t now, uint32_t period)
{
	uint32_t interval = period << 10;

	return now - now % interval + interval;
}

/* 0x15b26: the next header time if it comes before deadline, else 0. */
uint64_t wlanPsHeaderBefore(uint64_t deadline)
{
	uint64_t next;

	wlanPsPrepareHeader(&next);
	if (next < deadline)
		return next;
	return 0;
}

/* 0x15b48: nothing is left to send or to hand to the host. */
int wlanStaPsTxIdle(void)
{
	return macTxIdle() && MAC_010_QUEUE_HEAD == MAC_014_QUEUE_TAIL &&
	       mdevIoctl(wlanSdioDev, 8, 0) == 0 && wlanTxInFlightFrames() == 0 &&
	       wlanHostQueueDepth(0) == 0 && wlanHostQueueDepth(0) == 0 && wlanHostSleepPending == 0;
}

void wlanStaPsRequestSleep(void);

/* 0x15b94 */
void wlanStaPsSleepRetryExpired(uint32_t timer, uint32_t argument)
{
	if (wlanPsRetryTimer != timer)
		return;
	wlanPsRetryTimer = 0;
	wlanStaPsRequestSleep();
}

static inline __attribute__((always_inline)) int wlanStaPsPoolFull(struct OsaBufferPool *pool)
{
	int blocks = osa_BufferClone(pool);
	int free = osa_PoolQuery(pool, 8);

	return blocks == free + osa_PoolQuery(pool, 16);
}

/*
 * 0x15ba8: ask the MAC to sleep for the period, or tell the control task it
 * cannot; retries every WLAN_COEX_SLEEP_RETRY ticks.
 */
void wlanStaPsRequestSleep(void)
{
	if ((wlanStaPsTxIdle() == 1 && wlanStaPsPoolFull(&wlanBlockPool28) &&
	     wlanStaPsPoolFull(&wlanBlockPool7) && wlanStaPsPoolFull(&wlanReplyFramePool)) ||
	    wlanPsHostBusy != 0 || wlanPsSleepAllowance == 0) {
		wlanRaiseEvent(WLAN_EVENT_SLEEP_REFUSED);
		return;
	}
	macTxPostEvent(1);
	wlanPsSleepAllowance--;
	wlanPsArmRetryTimer(WLAN_COEX_SLEEP_RETRY, wlanStaPsSleepRetryExpired);
}

/* 0x15c40: the access point answered a PS-Poll. */
void wlanPsPollReplied(void)
{
	if (wlanPsLegacyPollMode() && !wlanPsDisabled())
		wlanPsPollReplyTime = romGetTimerTimestamp();
}

/* 0x15c60 */
void wlanPsHostFrameSeen(void)
{
	wlanRaiseEvent(25);
}

/* 0x15c6a: a frame from the access point after a PS-Poll; poll again for more. */
void wlanPsPollFrameReceived(struct WlanConnection *connection, int moreData, int subtype)
{
	if (wlanPsDisabled() || !wlanPsLegacyPollMode() || !wlanPsSleepStateReached())
		return;
	if (wlanPsAltModeActive != 0 && wlanPsHostBusy != 0)
		return;
	if (wlanPsPollSentTime == 0 || wlanPsPollSentTime >= wlanPsPollReplyTime)
		return;
	if (wlanDmaRingsHaveRoom() == 1)
		return;
	if (moreData != 0)
		wlanPsEnterSleep(connection, 0);
	else if (mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PENDING, 0) != 0)
		wlanPsHostWakePending = 1;
	else
		wlanRaiseEvent(26);
	if (wlanCoexUseStationMode())
		return;
	wlanPsLastTxComplete = moreData == 0;
	if (subtype == 4 || subtype == 12) {
		wlanPsPollSkips++;
		return;
	}
	wlanPsPollSkips = 0;
	wlanPsArmWakeTimer();
}

/* 0x15d48: PM=0 with the access point, from the periodic states. */
const int32_t *wlanStaPsPeriodicIeeeExit(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;

	switch (*event) {
	case 43:
		if (machine->resume == &machine->active) {
			wlanStaPsWakeAp(machine);
		} else if (macTxGetPowerSave() == 0) {
			machine->ieeeTries = 0;
			machine->ieeeState = 1;
			macTxSetPowerSave(1);
			wlanStaPsSendIeeeNull(connection, 1, 0);
		}
		romHsmTransition(machine, machine->resume);
		return NULL;
	case 44:
	case 45:
		machine->resume = &machine->active;
		machine->ieeeState = 0;
		macTxSetPowerSave(0);
		sleepCalibrationUpdate();
		sleepWake();
		machine->eventPending = 0;
		romHsmTransition(machine, machine->resume);
		return NULL;
	case WLAN_STA_PS_EVENT_HOST_CONFIRMED:
		machine->exitEventDue = 1;
		return NULL;
	case ROM_HSM_EXIT:
		machine->exitEventDue = 0;
		break;
	case ROM_HSM_ENTRY:
		machine->ieeeTries = 0;
		break;
	default:
		return event;
	}
	osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
	return NULL;
}

/* 0x15e00: PM=1 with the access point, from the periodic states. */
const int32_t *wlanStaPsPeriodicIeeeNull(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;

	switch (*event) {
	case 39:
	case 40:
		return NULL;
	case ROM_HSM_EXIT:
		machine->exitEventDue = 0;
		break;
	case ROM_HSM_ENTRY:
		machine->wakeBits = macTxAcquireWakeBit();
		break;
	case WLAN_STA_PS_EVENT_HOST_CONFIRMED:
		machine->exitEventDue = 1;
		return NULL;
	case 43:
		machine->slotHeld = 0;
		macTxReleaseWakeBits(&machine->wakeBits);
		if (machine->resume == &machine->active) {
			macTxSetPowerSave(1);
			wlanStaPsWakeAp(machine);
		} else if (machine->exitEventDue) {
			wlanRaiseEvent(9);
		}
		romHsmTransition(machine, machine->resume);
		return NULL;
	case 44:
	case 45:
		sleepCalibrationUpdate();
		sleepWake();
		tx_timer_deactivate(&machine->nullTimer);
		wlanPsSetPowerState(6);
		machine->ieeeState = 0;
		macTxSetPowerSave(0);
		macTxReleaseWakeBits(&machine->wakeBits);
		if (machine->resume != &machine->active)
			wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
		machine->eventPending = 0;
		machine->resume = &machine->active;
		romHsmTransition(machine, &machine->active);
		break;
	case 47:
		wlanPsNullRetries++;
		return NULL;
	default:
		return event;
	}
	osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
	return NULL;
}

/* 0x15f02: asleep for the period; event 24 tries to sleep until the next one. */
const int32_t *wlanStaPsPeriodicSleep(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	struct RomHsmState *periodic = &machine->periodic;
	uint64_t now;
	uint64_t wake;
	uint8_t previous[4];
	UINT irq;

	switch (*event) {
	case 11:
	case 12:
	case 21:
	case 22:
	case 25:
	case 26:
	case 32:
	case 33:
	case 34:
	case 38:
		return NULL;
	case 23:
		machine->sleepRequested = 1;
		if (wlanPsRetries != 15)
			return NULL;
		wlanPsRetries = 0;
		wlanPsCancelRetryTimer();
		goto request;
	case ROM_HSM_EXIT:
		sleepWake();
		return NULL;
	case ROM_HSM_ENTRY:
		wlanPsClearHostEvent();
		wlanPsCancelRetryTimer();
		wlanPsPeriodEntries++;
		wlanPsSleepAllowance = 10;
request:
		if (machine->servicePeriod == 0)
			wlanStaPsRequestSleep();
		else
			wlanCoexRequestSleep();
		return NULL;
	case WLAN_STA_PS_EVENT_STOP:
		wlanStaPsWakeAp(machine);
		wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
		romHsmTransition(machine, &machine->active);
		return NULL;
	case 3:
		if (wlanSavedMgmtPriority != 0xff) {
			osaTaskSetPriority(macMgmtTask, wlanSavedMgmtPriority, previous);
			wlanSavedMgmtPriority = 0xff;
		}
		goto awake;
	case WLAN_STA_PS_EVENT_IEEE_ENTER:
		machine->slotHeld = 1;
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_EXIT:
		machine->resume = periodic;
		romHsmTransition(machine, &machine->periodicIeeeExit);
		return NULL;
	case WLAN_EVENT_SLEEP_REFUSED:
		break;
	default:
		return event;
	}

	if (wlanCoexActive()) {
		wlanCoexHandleEvent(connection, WLAN_EVENT_SLEEP_REFUSED);
	} else if (mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_BUSY, 0) != 0) {
		sleepCalibrationUpdate();
		goto awake;
	}
	if (machine->headerPending == 1 && machine->sleepRequested == 1) {
		if (machine->headerTime >= wlanPsHeaderDeadline) {
			wlanPsHeaderDeadline = 0;
			machine->headerPending = 0;
		}
		machine->sleepRequested = 0;
	}
	if (wlanPsCalibrationDue == 1) {
		sleepCalibrationUpdate();
		wlanPsCalibrationDue = 0;
	}
	now = romGetTimerTimestamp();
	wlanPsPeriodNow = now;
	if (now >= machine->nextWake || wlanPsHostBusy != 0)
		goto awake;
	wlanPsPeriodSleepTime = (uint32_t)(machine->nextWake - wlanPsPeriodNow);
	irq = sysIrqDisable();
	if (machine->headerPending == 1 || wlanPsPeriodSleepTime < 5500 ||
	    (uint32_t)(wlanPsAltModeActive << 10) < wlanPsPeriodSleepTime)
		goto retry;
	if (wlanCoexUseP2pMode() == 1 && wlanPsIeeeFlagAtSleep != wlanPsIeeeFlag)
		goto retry;
	if ((wlanPsUapsdPartial() && wlanPsPeriodHold != 0) || wlanPsSleepAllowance == 0 ||
	    machine->servicePeriod != 0 || mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PENDING, 0) != 0)
		goto retry;
	if (wlanSessionId != 0 && osa_PoolQuery(&wlanTxFramePool, 2) != 0) {
		sysIrqRestore(irq);
		goto awake;
	}
	wake = machine->nextWake - wlanPsWakeAdvance;
	wlanPsIeeeFlag = 0;
	wlanPsIeeeFlagAtSleep = 0;
	wlanPsSetPowerState(6);
	osaPauseTimers();
	if (sleepEnter(&wake, (uint16_t)(sleepWakeLatency + 30), 1000, wlanPsAltModeActive << 10,
		       (uint8_t)machine->servicePeriod) == 0) {
		sysIrqRestore(irq);
		osaResumeTimers();
		goto awake;
	}
	sysIrqRestore(irq);
	return NULL;

retry:
	sysIrqRestore(irq);
	wlanPsRetries = 15;
	uapSqramFilterEntry(3);
	wlanPsArmRetryTimer((uint16_t)wlanPsPeriodSleepTime, wlanPsPeriodRetryExpired);
	wlanPsIeeeFlag = 0;
	wlanPsIeeeFlagAtSleep = 0;
	return NULL;

awake:
	romHsmTransition(machine, periodic);
	return NULL;
}

/* 0x161a0: awake for the period; Enter once nothing is left to do. */
const int32_t *wlanStaPsPeriodic(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	struct RomHsmState *enter = &machine->periodicEnter;
	uint32_t ieeeFlag;
	uint32_t window;
	uint64_t next;
	uint32_t sleepTime;

	switch (*event) {
	case ROM_HSM_ENTRY:
		macTxWakeup();
		if (machine->restartDue) {
			machine->restartDue = 0;
			wlanStaPsRestart(machine);
			if (wlanPsAltModeActive == 0)
				return NULL;
		}
		machine->servicePeriod = 0;
		wlanPsDataPending = 0;
		wlanPsIeeeFlag = 0;
		wlanTxPowerSaveCount = 0;
		wlanPsIeeeFlagAtSleep = 0;
		wlanPsConfirmArmed = 0;
		wlanPsLastTxComplete = 0;
		wlanPsPollReplyTime = 0;
		wlanPsPollSentTime = 0;
		wlanPsHostWakePending = 0;
		wlanPsPollFlag = 0;
		wlanPsHostBusy = 0;
		wlanPsPeriodStart = romGetTimerTimestamp();
		next = wlanPsNextPeriod(wlanPsPeriodStart, wlanPsAltModeActive);
		machine->nextWake = next;
		if (wlanPsPeriodStart + wlanPsWakeWindow * 1000 >= next)
			machine->nextWake += (int32_t)(wlanPsAltModeActive << 10);
		window = wlanPsWakeWindow * 1000;
		next = machine->nextWake;
		sleepTime = (uint32_t)(next - wlanPsPeriodStart - window);
		wlanPsPeriodSleepTime = sleepTime;
		if (sleepTime < 5000) {
			machine->nextWake = next + (int32_t)(wlanPsAltModeActive << 10);
			wlanPsPeriodSleepTime = (wlanPsAltModeActive << 10) + sleepTime;
		}
		wlanPsArmRetryTimer((uint16_t)wlanPsPeriodSleepTime, wlanPsPeriodTimerExpired);
		if (machine->lastCalibration + wlanPsCalibrationInterval * 1000 < wlanPsPeriodStart) {
			machine->lastCalibration = wlanPsPeriodStart;
			sleepCalibrationKick(2);
			wlanPsCalibrations++;
			wlanPsCalibrationDue = 1;
		}
		wlanPsHeaderDeadline = wlanPsHeaderBefore(machine->nextWake);
		machine->periodWake = machine->nextWake;
		if (wlanPsHeaderDeadline != 0) {
			machine->headerPending = 1;
			if (machine->sleepRequested == 1) {
				if (machine->headerTime >= wlanPsHeaderDeadline) {
					wlanPsHeaderDeadline = 0;
					machine->headerPending = 0;
				}
				machine->sleepRequested = 0;
			}
		}
		wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 1);
		if (machine->hostState == 2)
			machine->hostState = 0;
		if (wlanSubscribeEventsPending(connection))
			wlanPostConnectionEvent(connection, 0x08000000);
		if (wlanPsEvaluate(connection, 2) == 0)
			return NULL;
		goto evaluated;
	case ROM_HSM_EXIT:
		wlanPsIeeeFlagAtSleep = wlanPsIeeeFlag;
		wlanPsCancelRetryTimer();
		wlanPsPollStop();
		return NULL;
	case WLAN_STA_PS_EVENT_STOP:
		wlanStaPsWakeAp(machine);
		wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
		romHsmTransition(machine, &machine->active);
		return NULL;
	case 21:
		ieeeFlag = wlanPsIeeeFlag;
		if (ieeeFlag == 0 || wlanPsDataPending == 0 || !wlanPsIdle() ||
		    wlanPsConfirmArmed != 1 || wlanStaPsTxIdle() != 1 || wlanDmaRingsHaveRoom() == 1 ||
		    wlanPsNullSuppressed != 0) {
			wlanPsTriggerSent();
			return NULL;
		}
		wlanPsCancelRetryTimer();
		wlanPsPeriodDataSleeps++;
		goto sleep;
	case 22:
		wlanPsCancelRetryTimer();
		wlanPsHostBusy++;
		if (wlanPsPollBusy() || wlanCoexDeferred()) {
			wlanPsArmRetryTimer(2000, wlanPsPeriodTimerExpired);
			return NULL;
		}
		wlanCoexHandleEvent(connection, 22);
sleep:
		wlanPsIeeeFlagAtSleep = wlanPsIeeeFlag;
		break;
	case 23:
		machine->sleepRequested = 1;
		return NULL;
	case WLAN_EVENT_SLEEP_REFUSED:
		return NULL;
	case 25:
		if (wlanPsNullSuppressed != 0 || macTxPathReady() != 1)
			return NULL;
		wlanPsConfirmArmed = 1;
		if (!wlanCoexUseStationMode() || wlanPsLastTxComplete == 0 || !wlanPsIdle())
			return NULL;
		break;
	case 26:
		if (!wlanCoexUseStationMode() || wlanDmaRingsHaveRoom() != 0 || wlanPsDataPending == 0)
			return NULL;
		wlanPsLastTxComplete = 1;
		wlanPsEvaluate(connection, 4);
		if (wlanPsConfirmArmed == 0 || !wlanPsIdle())
			return NULL;
		break;
	case 32:
	case 33:
		wlanPsPollHandleEvent(connection, *event);
		return NULL;
	case 34:
		if (wlanPsCoexWindowEnd() == 0)
			return NULL;
		wlanPsCancelRetryTimer();
		wlanPsIeeeFlagAtSleep = wlanPsIeeeFlag;
		machine->servicePeriod = 1;
		break;
	case 38:
evaluated:
		if (*event == 38)
			wlanPsEvaluate(connection, 3);
		if (wlanCoexUseStationMode())
			wlanPsEnterSleep(connection, 0);
		macTxPostEvent(1);
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_ENTER:
		machine->slotHeld = 1;
		return NULL;
	case WLAN_STA_PS_EVENT_IEEE_EXIT:
		machine->resume = &machine->periodic;
		romHsmTransition(machine, &machine->periodicIeeeExit);
		return NULL;
	default:
		return event;
	}
	romHsmTransition(machine, enter);
	return NULL;
}

/* 0x1654e: tell the host, then sleep for the period. */
const int32_t *wlanStaPsPeriodicEnter(struct WlanStaPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	struct RomHsmState *target;

	switch (*event) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		break;
	default:
		return event;
	}
	if (machine->slotHeld) {
		sysEnablePendingEvent();
		machine->resume = &machine->periodicEnter;
		target = &machine->periodicIeeeNull;
	} else if (wlanPsTriggerMode() != 0 && (machine->servicePeriod | wlanPsHostBusy) != 0) {
		target = &machine->periodicSleep;
	} else {
		wlanCoexHandleEvent(connection, 50);
		wlanPsPeriodAnnounced = 1;
		if (mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_BUSY, 0) != 0)
			target = &machine->periodic;
		else
			target = &machine->periodicSleep;
	}
	romHsmTransition(machine, target);
	return NULL;
}

/* 0x165c4 */
UINT wlanStaPsCreate(void)
{
	wlanStaPsInitStates(&wlanStaPs);
	romHsmStart(&wlanStaPs, &wlanStaPs.active);
	tx_timer_create(&wlanStaPs.confirmTimer, "SleepConfirmTmr", wlanPsSleepConfirmTimerCb, 0,
			WLAN_STA_PS_CONFIRM_TICKS, 0, 0);
	return tx_timer_create(&wlanStaPs.nullTimer, "NullPktDoneTmr", wlanPsApNullPktDoneTimerCb, 0,
			       WLAN_STA_PS_NULL_TICKS, 0, 0);
}

/* 0x16606: a beacon from the access point. */
void wlanStaPsBeaconSeen(struct WlanConnection *connection, const void *beacon, int dtimOnly)
{
	int bits;
	int dtim;

	wlanStaPs.beaconsSeen++;
	if (!wlanPsEnabled())
		return;
	bits = wlanPsResetDescriptor(connection, beacon);
	dtim = bits & 2;
	bits &= 9;
	if (dtimOnly != 0) {
		if (dtim == 0)
			return;
		bits = 0;
	}
	wlanPsBeaconTraffic(connection, bits, dtim, 0);
}

/* 0x16646 */
void wlanStaPsReset(void)
{
	osaSemaphoreRelease(wlanPsResetDone);
	romHsmStart(&wlanStaPs, &wlanStaPs.active);
	wlanPsDataPending = 0;
	wlanPsIeeeFlag = 0;
	wlanTxPowerSaveCount = 0;
	wlanPsHoldSeconds = 30;
}

static inline __attribute__((always_inline)) unsigned int wlanPsHostConfigValue(const uint8_t *value)
{
	return value[1] << 8 | value[0];
}

/* 0x166b8: the host's power-save parameters. */
void wlanStaPsLoadHostConfig(void)
{
	unsigned int value;

	value = wlanPsHostConfigValue(wlanPsHostConfig.dtimPolicy);
	if (value != 0 && (value <= 5 || value >= 0xfffd))
		wlanPsDtimPolicy = value;
	value = wlanPsHostConfigValue(wlanPsHostConfig.holdSeconds);
	if (value != 0)
		wlanPsHoldSeconds = value;
	value = wlanPsHostConfigValue(wlanPsHostConfig.nullWindow);
	if (value != 0)
		wlanPsNullWindow = value;
	wlanPsHeaderSpacing = wlanPsHostConfigValue(wlanPsHostConfig.headerSpacing);
	wlanPsPollWindow = wlanPsHostConfigValue(wlanPsHostConfig.pollWindow);
	if (wlanPsHostMode == 0)
		wlanPsBtShareMode = wlanPsHostConfigValue(wlanPsHostConfig.btShare) == 2;
}

/* 0x16724: tell the host of the wake-up or the sleep once, or again when forced. */
void wlanStaPsTellHost(struct WlanConnection *connection, uint32_t event, int force)
{
	if (event != wlanPsLastHostEvent || force != 0)
		wlanPostConnectionEvent(connection, event);
	wlanPsLastHostEvent = event;
}

/* 0x1673e: into the periodic states with PM=1 already set. */
void wlanStaPsStartPeriodic(struct WlanStaPsMachine *machine)
{
	struct WlanConnection *connection = machine->connection;
	uint64_t now;

	wlanPsSetPowerState(5);
	wlanPsPeriodWakes = 0;
	wlanPsHostBusy = 0;
	wlanPsCalibrations = 0;
	wlanPsPeriodSleeps = 0;
	wlanPsPeriodMisses = 0;
	wlanPsPeriodAborts = 0;
	wlanPsConfirmArmed = 0;
	if (macTxGetPowerSave() == 0)
		wlanStaPsSendNull(connection, 1);
	now = romGetTimerTimestamp();
	machine->periodWake = now;
	machine->lastCalibration = now;
	macMgmtValidateTable(4);
	wlanPsEvaluate(connection, 0);
	wlanStaPsTellHost(connection, WLAN_EVENT_HOST_PS_AWAKE, 0);
}

/* 0x1679a: into the periodic states from Active. */
void wlanStaPsBeginPeriodic(struct WlanStaPsMachine *machine)
{
	struct WlanConnection *connection;
	uint64_t now;

	wlanPsPeriodWakes = 0;
	wlanPsHostBusy = 0;
	wlanPsCalibrations = 0;
	wlanPsPeriodSleeps = 0;
	wlanPsPeriodMisses = 0;
	wlanPsPeriodAborts = 0;
	wlanPsConfirmArmed = 0;
	connection = machine->connection;
	wlanStaPsSendNull(connection, 1);
	now = romGetTimerTimestamp();
	machine->periodWake = now;
	machine->lastCalibration = now;
	wlanPsEvaluate(connection, 0);
}

/* 0x167d8 */
void wlanStaPsInitStates(struct WlanStaPsMachine *machine)
{
	romHsmStateInit(&machine->root, NULL, (RomHsmHandler)wlanStaPsRoot);
	romHsmStateInit(&machine->active, &machine->root, (RomHsmHandler)wlanStaPsActive);
	romHsmStateInit(&machine->powerSave, &machine->root, (RomHsmHandler)wlanStaPsPowerSave);
	romHsmStateInit(&machine->ieee, &machine->root, (RomHsmHandler)wlanStaPsIeee);
	romHsmStateInit(&machine->ieeeNull, &machine->ieee, (RomHsmHandler)wlanStaPsIeeeNull);
	romHsmStateInit(&machine->ieeeExit, &machine->ieee, (RomHsmHandler)wlanStaPsIeeeExit);
	romHsmStateInit(&machine->enter, &machine->powerSave, (RomHsmHandler)wlanStaPsEnter);
	romHsmStateInit(&machine->sleep, &machine->powerSave, (RomHsmHandler)wlanStaPsSleep);
	romHsmStateInit(&machine->confirm, &machine->powerSave, (RomHsmHandler)wlanStaPsConfirm);
	romHsmStateInit(&machine->periodicIeeeNull, &machine->ieee,
			(RomHsmHandler)wlanStaPsPeriodicIeeeNull);
	romHsmStateInit(&machine->periodicIeeeExit, &machine->ieee,
			(RomHsmHandler)wlanStaPsPeriodicIeeeExit);
	romHsmStateInit(&machine->periodicSleep, &machine->powerSave,
			(RomHsmHandler)wlanStaPsPeriodicSleep);
	romHsmStateInit(&machine->periodic, &machine->powerSave, (RomHsmHandler)wlanStaPsPeriodic);
	romHsmStateInit(&machine->periodicEnter, &machine->powerSave,
			(RomHsmHandler)wlanStaPsPeriodicEnter);
}
