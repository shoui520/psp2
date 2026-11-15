/*
 * The IBSS's power-save state machine (0x3c20c..0x3c702): the reduced form of the station's
 * (wlan/stapssm.c), driven through mdevRomQueryState() from wlanPsPostEvent(). The names of the
 * firmware's own placeholders are kept (wlanPs*Buffer); the states are named in robin/psctl.h.
 */
#include <stddef.h>
#include "robin/btrom.h"
#include "robin/coex.h"
#include "robin/hostps.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/psctl.h"
#include "robin/pssm.h"
#include "robin/rom.h"
#include "robin/scan.h"
#include "robin/sleep.h"
#include "robin/subscribe.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/threadx.h"
#include "robin/timer.h"
#include "robin/txhold.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"
#include "robin/macdefaults.h"

/* The events the machine is sent. */
#define WLAN_IBSS_PS_EVENT_SLEEP	1	/* no traffic: sleep */
#define WLAN_IBSS_PS_EVENT_WAKE		2	/* the host wakes the station */
#define WLAN_IBSS_PS_EVENT_ENTER	3	/* Enter is done: sleep */
#define WLAN_IBSS_PS_EVENT_CONFIRMED	9	/* the host confirmed the sleep (WLAN_PS_EVENT_SLEEP_CONFIRMED) */
#define WLAN_IBSS_PS_EVENT_TIMEOUT	10	/* "SleepConfirmTmr" expired */
#define WLAN_IBSS_PS_EVENT_HOLD_LEAVE	43	/* the held frames are sent: IeeeHold returns */

#define WLAN_IBSS_PS_CONFIRM_TICKS	4

/* Not converted yet. */

struct WlanIbssPsMachine wlanIbssPsMachine __attribute__((section(".itcm.bss")));	/* 0x5963c */

/* 0x3c20c: sleep until the next beacon or timer; 1 when a timer is too close. */
__attribute__((noinline)) int wlanPsCheckBuffer(void)
{
	uint32_t period = wlanLinkCheckPeriod();
	uint32_t irq = sysIrqDisable();
	uint64_t now = romGetTimerTimestamp();
	uint64_t timerDeadline;
	uint32_t wait = period - now % period;
	uint32_t next = osaGetNextTimerDeadline(&timerDeadline, 2, 1);
	uint32_t margin;

	sysIrqRestore(irq);
	margin = sleepWakeLatency + 345;
	if (next == 0 || next >= wait) {
		next = wait;
	} else if (osaExpireTimerWithin(next + margin) != 0) {
		wlanPsSetPowerState(4);
		return 1;
	}
	sleepWakeDeadline = now + next;
	wlanPsSetPowerState(1);
	return sleepEnter(&sleepWakeDeadline, (uint16_t)margin, 1000, period, 1) != 0;
}

/* 0x3c29e: the root; the machine starts in Off. */
const int32_t *wlanPsValidateBuffer(struct WlanIbssPsMachine *machine, const int32_t *event)
{
	if (*event == ROM_HSM_ENTRY)
		romHsmTransition(machine, &machine->off);
	return NULL;
}

/* 0x3c2b6: power save is off. */
const int32_t *wlanPsPrepareBuffer(struct WlanIbssPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	struct RomHsmState *target;

	switch (*event) {
	case WLAN_PS_EVENT_IEEE_ENTER:
		wlanPsClearTable(connection, 5);
		machine->resume = &machine->off;
		target = &machine->ieeeHold;
		break;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		wlanPsSetOverride(1);
		macTxSetPowerSave(0);
		macMgmtValidateDescriptor(0);
		wlanPsSetPowerState(0);
		macMgmtValidateTable(6);
		return NULL;
	case WLAN_IBSS_PS_EVENT_SLEEP:
		macTxSetPowerSave(1);
		macMgmtValidateDescriptor(1);
		wlanPsHostCmdRespond(connection);
		target = &machine->sleep;
		break;
	case 43:
	case 44:
	case 45:
		return NULL;
	default:
		return event;
	}
	romHsmTransition(machine, target);
	return NULL;
}

/* 0x3c33a: power save is on. */
const int32_t *wlanPsDispatchBuffer(struct WlanIbssPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;

	switch (*event) {
	case WLAN_PS_EVENT_IEEE_ENTER:
	case ROM_HSM_EXIT:
	case 43:
	case 44:
	case 45:
		return NULL;
	case ROM_HSM_ENTRY:
		wlanPsSetOverride(0);
		macMgmtValidateTable(5);
		fmCoexComputeBuffer();
		return NULL;
	case WLAN_IBSS_PS_EVENT_WAKE:
		sleepCalibrationUpdate();
		sleepWake();
		wlanPsHostCmdRespond(connection);
		romHsmTransition(machine, &machine->off);
		return NULL;
	}
	return event;
}

/* 0x3c394: the sleep awaits the host's confirmation. */
const int32_t *wlanPsParseBuffer(struct WlanIbssPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	TX_TIMER *timer = &machine->confirmTimer;
	struct RomHsmState *target;
	int busy;

	switch (*event) {
	case WLAN_IBSS_PS_EVENT_CONFIRMED:
		tx_timer_deactivate(timer);
		MAC_REG(0x300) &= ~8u;
		if (macMgmtParseDescriptor() != 0 && wlanPsConfigureBuffer() &&
		    !wlanSubscribeEventsPending(connection) &&
		    mdevIoctl(wlanSdioDev, 0x46, 0) == 0) {
			wlanPsClearTable(connection, 4);
			target = &machine->enter;
			break;
		}
		MAC_REG(0x300) |= 8;
		/* fall through */
	case WLAN_IBSS_PS_EVENT_TIMEOUT:
		wlanPsSetPowerState(1);
		target = &machine->sleep;
		break;
	case WLAN_PS_EVENT_IEEE_ENTER:
		tx_timer_deactivate(timer);
		wlanPsClearTable(connection, 5);
		machine->resume = &machine->off;
		target = &machine->ieeeHold;
		break;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		busy = macTxPoolsIdle() == 0;
		if ((wlanHostSleepArmed() || machine->awakeReported != 0) && !busy) {
			if (wlanHostSleepArmed()) {
				wlanRaiseEvent(WLAN_IBSS_PS_EVENT_CONFIRMED);
			} else {
				wlanPostConnectionEvent(connection, WLAN_EVENT_HOST_PS_SLEEP);
				tx_timer_deactivate(timer);
				tx_timer_change(timer, WLAN_IBSS_PS_CONFIRM_TICKS, 0);
				tx_timer_activate(timer);
			}
			return NULL;
		}
		wlanPsSetPowerState(1);
		target = &machine->sleep;
		break;
	default:
		return event;
	}
	romHsmTransition(machine, target);
	return NULL;
}

/* 0x3c48c: the sleep is being prepared. */
const int32_t *wlanPsBuildBuffer(struct WlanIbssPsMachine *machine, const int32_t *event)
{
	struct RomHsmState *target = &machine->sleep;

	switch (*event) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		sleepCalibrationUpdate();
		if (wlanPsCheckBuffer() != 0)
			return NULL;
		break;
	case WLAN_IBSS_PS_EVENT_ENTER:
		break;
	case WLAN_PS_EVENT_IEEE_ENTER:
		machine->resume = &machine->sleep;
		target = &machine->ieeeHold;
		break;
	default:
		return event;
	}
	romHsmTransition(machine, target);
	return NULL;
}

/* 0x3c4cc: the station sleeps. */
const int32_t *wlanPsComputeBuffer(struct WlanIbssPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	struct RomHsmState *target;
	uint32_t irq;
	uint32_t posted;

	switch (*event) {
	case 18:
		if (wlanMefConfig.hostState != 0)
			return NULL;
		goto dispatch;
	case 19:
		macTxPostEvent(1);
		return NULL;
	case 20:
		irq = sysIrqDisable();
		if (wlanMefConfig.hostState != 0 || wlanMefConfig.hostEvents != 0) {
			wlanPsArmWakeTimer();
			sysIrqRestore(irq);
			return NULL;
		}
		sysIrqRestore(irq);
		goto dispatch;
	case WLAN_PS_EVENT_IEEE_ENTER:
		wlanPsClearTable(connection, 5);
		machine->resume = &machine->sleep;
		target = &machine->ieeeHold;
		goto transition;
	case ROM_HSM_EXIT:
		wlanPsCancelWakeTimer();
		return NULL;
	case ROM_HSM_ENTRY:
		sleepCalibrationKick(2);
		machine->awakeReported = 0;
		if (wlanPsGetPowerState() == 2 || wlanSubscribeEventsPending(connection) ||
		    !wlanHostSleepArmed()) {
			machine->awakeReported = 1;
			wlanPostConnectionEvent(connection, WLAN_EVENT_HOST_PS_AWAKE);
		}
		if (!wlanSubscribeEventsPending(connection))
			return NULL;
		posted = WLAN_EVENT_SUBSCRIBED;
		break;
	case 5:
		if (machine->awakeReported != 0)
			return NULL;
		machine->awakeReported = 1;
		posted = WLAN_EVENT_HOST_PS_AWAKE;
		break;
	default:
		return event;
	}
	wlanPostConnectionEvent(connection, posted);
	return NULL;

dispatch:
	if (macMgmtDispatchDescriptor() == 0)
		return NULL;
	target = &machine->confirm;
transition:
	romHsmTransition(machine, target);
	return NULL;
}

/* 0x3c5b6: the 802.11 power-management null frames. */
const int32_t *wlanPsQueryBuffer(struct WlanIbssPsMachine *machine, const int32_t *event)
{
	uint32_t active;

	switch (*event) {
	case WLAN_IBSS_PS_EVENT_SLEEP:
		machine->resume = &machine->sleep;
		wlanPsHostCmdRespond(machine->connection);
		return NULL;
	case WLAN_IBSS_PS_EVENT_WAKE:
		machine->resume = &machine->off;
		wlanPsHostCmdRespond(machine->connection);
		return NULL;
	case ROM_HSM_EXIT:
		active = 0;
		break;
	case ROM_HSM_ENTRY:
		active = 1;
		break;
	default:
		return event;
	}
	wlanPsSetIeeeActive(active);
	return NULL;
}

/* 0x3c602: the null frame is held until the frames of the peers are sent. */
const int32_t *wlanPsResetBuffer(struct WlanIbssPsMachine *machine, const int32_t *event)
{
	switch (*event) {
	case WLAN_IBSS_PS_EVENT_CONFIRMED:
	case WLAN_IBSS_PS_EVENT_TIMEOUT:
		return NULL;
	case ROM_HSM_ENTRY:
		machine->wakeBits = macTxAcquireWakeBit();
		/* fall through */
	case ROM_HSM_EXIT:
		osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
		return NULL;
	case WLAN_IBSS_PS_EVENT_HOLD_LEAVE:
		macTxReleaseWakeBits(&machine->wakeBits);
		wlanPsSetPowerState(1);
		wlanPsArmIdleWakeTimer();
		romHsmTransition(machine, machine->resume);
		return NULL;
	}
	return event;
}

/* 0x3c69c */
__attribute__((noinline)) void wlanPsScheduleBuffer(struct WlanIbssPsMachine *machine)
{
	romHsmStateInit(&machine->root, NULL, (RomHsmHandler)wlanPsValidateBuffer);
	romHsmStateInit(&machine->off, &machine->root, (RomHsmHandler)wlanPsPrepareBuffer);
	romHsmStateInit(&machine->ieee, &machine->root, (RomHsmHandler)wlanPsQueryBuffer);
	romHsmStateInit(&machine->ieeeHold, &machine->ieee, (RomHsmHandler)wlanPsResetBuffer);
	romHsmStateInit(&machine->powerSave, &machine->root, (RomHsmHandler)wlanPsDispatchBuffer);
	romHsmStateInit(&machine->confirm, &machine->powerSave, (RomHsmHandler)wlanPsParseBuffer);
	romHsmStateInit(&machine->enter, &machine->powerSave, (RomHsmHandler)wlanPsBuildBuffer);
	romHsmStateInit(&machine->sleep, &machine->powerSave, (RomHsmHandler)wlanPsComputeBuffer);
}

/* 0x3c65a */
int wlanPsSyncBuffer(void)
{
	wlanPsScheduleBuffer(&wlanIbssPsMachine);
	romHsmStart(&wlanIbssPsMachine, &wlanIbssPsMachine.off);
	return tx_timer_create(&wlanIbssPsMachine.confirmTimer, "SleepConfirmTmr",
			       wlanPsSleepConfirmTimerCb, 0, WLAN_IBSS_PS_CONFIRM_TICKS, 0, 0);
}

/* 0x3c686 */
int wlanPsApplyBuffer(void)
{
	return fmCoexQueryBuffer();
}

/* 0x3c68e */
void wlanPsResolveBuffer(void)
{
	romHsmStart(&wlanIbssPsMachine, &wlanIbssPsMachine.off);
}
