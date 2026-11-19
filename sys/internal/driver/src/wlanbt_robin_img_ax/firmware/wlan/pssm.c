/*
 * The power-save state machine of a connection (0x13410..0x14306). Off and
 * On, and below On: Confirm tells the host (event 0x2000, then its
 * confirmation) and the peers (a power-management null frame, and a
 * broadcast null frame announcing the sleep) before Sleep puts the device to
 * sleep; Awake runs until the next sleep is due. Ieee drives the 802.11
 * power-management exchange with the access point. The sleep times come from
 * the inactivity timer and the DTIM beacons (struct WlanPsSleep). The
 * filename is inferred; no source filename survives.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/wmm.h"
#include "robin/hostps.h"
#include "robin/swcrypto.h"
#include "robin/coex.h"
#include "robin/stapssm.h"
#include "robin/conn.h"
#include "robin/psctl.h"
#include "robin/dmaring.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/pspoll.h"
#include "robin/pssm.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/threadx.h"
#include "robin/timer.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wlanrx.h"
#include "robin/wlantask.h"

/* How long before a beacon the MAC wakes (us), and the margins around it. */
#define MAC_WAKE_LEAD		MAC_REG(0x634)
#define WLAN_PS_WAKE_LEAD	0x1f0
#define WLAN_PS_GUARD		2000
#define WLAN_PS_MIN_SLEEP	2945

/* Timer ticks (ms). */
#define WLAN_PS_CONFIRM_TICKS	2000
#define WLAN_PS_NULL_TICKS	100

/* macTxSendNullFrame() kinds. */
#define WLAN_NULL_KIND_IEEE	19
#define WLAN_NULL_KIND_SLEEP	21

/* The sleep announcement: a broadcast null data frame with its duration. */
#define WLAN_PS_ANNOUNCE_SIZE	300

/* Not converted yet; provisional names. */
uint32_t wlanPsSleepTimerEnd __attribute__((section(".sqram1.bss")));			/* SQRAM 0xc000fb00 */
uint64_t wlanPsSleepTimerStart __attribute__((section(".sqram1.bss")));		/* SQRAM 0xc000fb08 */
#define WLAN_PS_SLEEP_DURATION_MAX	0x7ff0	/* the longest sleep a peer may announce */
static const uint8_t wlanPsBroadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };	/* 0x53d4a */
int wlanWmmEnabled(void);				/* 0x41cb8 */

static inline __attribute__((always_inline)) struct WlanPsSleep *wlanPsSleepOf(struct WlanConnection *connection)
{
	return &wlanConnectionInfo(connection)->sleep;
}

static inline __attribute__((always_inline)) int wlanPsAssociated(const struct WlanConnection *connection)
{
	return connection != NULL && (connection->flags2 & 0x10);
}

static inline __attribute__((always_inline)) void wlanPsRestartTimer(TX_TIMER *timer, ULONG ticks)
{
	tx_timer_deactivate(timer);
	tx_timer_change(timer, ticks, 0);
	tx_timer_activate(timer);
}

static inline __attribute__((always_inline)) void wlanPsSendNull(struct WlanConnection *connection,
								int powerManagement, int trigger,
								uint8_t kind)
{
	macTxSendNullFrame(connection, powerManagement, wlanWmmCurrentSlotNumber(), 0, trigger, kind);
}

/* 0x13410: tell the host the device is awake, once after a sleep it was told of. */
void wlanPsMachineReportAwake(struct WlanPsMachine *machine)
{
	sleepCalibrationKick(1);
	if (machine->awakeReportDue == 1) {
		machine->awakeReportDue = 0;
		machine->sleepReportDue = 1;
		wlanPostConnectionEvent(machine->connection, WLAN_EVENT_HOST_PS_AWAKE);
	}
}

/* 0x13438 */
const int32_t *wlanPsMachineRoot(struct WlanPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *first = wlanConnectionFirst(0);
	unsigned int tries;

	switch (*event) {
	case 17:
		machine->eventPending = 1;
		break;
	case 40:
		if (machine->ieeeState != 2 && machine->ieeeState != 1)
			break;
		tries = machine->ieeeTries++;
		if (tries >= 2)
			osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
		else if (wlanPsAssociated(first))
			wlanPsSendNull(first, machine->ieeeState == 1, 0, WLAN_NULL_KIND_IEEE);
		break;
	case ROM_HSM_ENTRY:
		romHsmTransition(machine, &machine->off);
		break;
	}
	return NULL;
}

/* 0x134c2 */
const int32_t *wlanPsMachineOff(struct WlanPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *first;

	switch (*event) {
	case WLAN_PS_EVENT_IEEE_ENTER:
		machine->resume = &machine->off;
		romHsmTransition(machine, &machine->ieeeNull);
		return NULL;
	case WLAN_PS_EVENT_IEEE_EXIT:
		machine->resume = &machine->off;
		romHsmTransition(machine, &machine->ieeeExit);
		return NULL;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		macTxWakeup();
		wlanPsSetOverride(1);
		wlanPsSetSleepLeft(1);
		wlanTxPowerSaveCount = 0;
		macTxSetPowerSave(0);
		if (machine->eventPending) {
			wlanRaiseEvent(17);
			machine->eventPending = 0;
		}
		wlanPsSetPowerState(0);
		return NULL;
	case 1:
		wlanPsHostCmdRespond(machine->connection);
		romHsmTransition(machine, &machine->on);
		return NULL;
	case 17:
		first = wlanConnectionFirst(0);
		if (first != NULL)
			wlanPostConnectionEvent(first, WLAN_EVENT_HOST_PS_PEER);
		return NULL;
	case 43:
	case 44:
	case 45:
		mdevRomQueryState(&wlanStaPs, *event, NULL);
		return NULL;
	}
	return event;
}

/* 0x13570: the awake time after an inactivity sleep ended. */
void wlanPsSleepTimerExpired(uint32_t timer, uint32_t argument)
{
	struct WlanPsSleep *sleep = wlanPsSleepOf((struct WlanConnection *)argument);

	(void)timer;
	wlanPsSleepTimerEnd = (uint32_t)romGetTimerTimestamp();
	sleep->sleepTimer = 0;
	sleep->sleepTimerArmed = 0;
	wlanRaiseEvent(18);
}

/* 0x13594 */
void wlanPsStartSleepTimer(struct WlanConnection *connection)
{
	struct WlanPsSleep *sleep = wlanPsSleepOf(connection);
	UINT posture;

	posture = sysIrqDisable();
	if (sleep->sleepTimer != 0) {
		osaCancelTimer(sleep->sleepTimer);
		sleep->sleepTimer = 0;
	}
	wlanPsSleepTimerStart = romGetTimerTimestamp();
	osaCreateTimer(wlanPsSleepTimerExpired, (uint32_t)connection, sleep->maxAwake,
		       &sleep->sleepTimer, 0);
	sleep->sleepTimerArmed = 1;
	sysIrqRestore(posture);
}

/* 0x135de: the inactivity timeout passed since the last frame, or it is rearmed for the rest. */
void wlanPsActivityTimerExpired(uint32_t timer, uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;
	struct WlanPsSleep *sleep = wlanPsSleepOf(connection);
	uint64_t now = romGetTimerTimestamp();
	uint64_t elapsed;

	(void)timer;
	if (sleep->lastActivity >= now)
		elapsed = ~sleep->lastActivity + now;
	else
		elapsed = now - sleep->lastActivity;
	if (elapsed >= sleep->inactivityTimeout) {
		sleep->activityTimer = 0;
		sleep->recentActivity = 0;
		if (sleep->mode == WLAN_PS_SLEEP_INACTIVITY)
			wlanPsStartSleepTimer(connection);
	} else {
		osaCreateTimer(wlanPsActivityTimerExpired, argument,
			       sleep->inactivityTimeout - (uint32_t)elapsed, &sleep->activityTimer, 0);
		sleep->recentActivity = 1;
	}
}

/* 0x1365a: a frame went out or came in; the device stays awake. */
void wlanPsActivity(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanPsSleep *sleep = &info->sleep;
	UINT posture;

	if (info->psMachine.sleepBlocked == 1)
		return;
	if (sleep->mode == WLAN_PS_SLEEP_INACTIVITY && sleep->sleepTimer != 0) {
		osaCancelTimer(sleep->sleepTimer);
		sleep->sleepTimer = 0;
	}
	posture = sysIrqDisable();
	sleep->lastActivity = romGetTimerTimestamp();
	if (sleep->activityTimer == 0)
		osaCreateTimer(wlanPsActivityTimerExpired, (uint32_t)connection,
			       sleep->inactivityTimeout, &sleep->activityTimer, 0);
	sleep->recentActivity = 1;
	sysIrqRestore(posture);
	wlanRaiseEvent(8);
}

/* 0x136c2 */
void wlanPsMachineStopTimers(struct WlanPsMachine *machine)
{
	struct WlanPsSleep *sleep = wlanPsSleepOf(machine->connection);

	tx_timer_deactivate(&machine->confirmTimer);
	tx_timer_deactivate(&machine->nullTimer);
	osaCancelTimer(sleep->sleepTimer);
	sleep->sleepTimer = 0;
	osaCancelTimer(sleep->activityTimer);
	sleep->activityTimer = 0;
}

/* 0x136f8 */
const int32_t *wlanPsMachineOn(struct WlanPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	struct WlanPsSleep *sleep = wlanPsSleepOf(connection);
	struct WlanConnection *first = wlanConnectionFirst(0);
	uint64_t now;
	uint32_t elapsed;

	switch (*event) {
	case WLAN_PS_EVENT_SLEEP_CONFIRMED:
		now = romGetTimerTimestamp();
		if (now >= sleep->sleptAt)
			elapsed = (uint32_t)(now - sleep->sleptAt);
		else
			elapsed = ~(uint32_t)(sleep->sleptAt - now);
		if (sleep->wakeLatency < elapsed)
			sleep->wakeLatency = elapsed;
		machine->hostConfirmed = 1;
		tx_timer_deactivate(&machine->confirmTimer);
		if (machine->announced == 1 && machine->state == &machine->confirm &&
		    machine->nullAcked == 1)
			romHsmTransition(machine, &machine->sleep);
		return NULL;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		wlanPsSetSleepLeft(0);
		wlanPsSetOverride(0);
		wlanTxPowerSaveCount = 1;
		if (machine->eventPending) {
			wlanRaiseEvent(17);
			machine->eventPending = 0;
		}
		machine->awakeReportDue = 1;
		MAC_WAKE_LEAD = WLAN_PS_WAKE_LEAD;
		wlanPsActivity(connection);
		break;
	case 2:
		sleepCalibrationUpdate();
		sleepWake();
		wlanPsMachineStopTimers(machine);
		wlanPsHostCmdRespond(connection);
		romHsmTransition(machine, &machine->off);
		return NULL;
	case 7:
		sleep->sleepAllowed = 1;
		if (first != NULL && memcmp(connection->interfaceAddress, first->interfaceAddress, 6) == 0)
			wlanTxPowerSaveCount++;
		/* fall through */
	case 3:
		if (sleep->mode == WLAN_PS_SLEEP_INACTIVITY && sleep->recentActivity == 0)
			wlanPsStartSleepTimer(connection);
		break;
	case 4:
		machine->awakeReportDue = 1;
		wlanPsMachineReportAwake(machine);
		break;
	case 5:
		machine->sleepReportDue = 1;
		break;
	case 6:
		sleep->sleepAllowed = 0;
		return NULL;
	case 10:
		machine->sleepBlocked = 0;
		/* fall through */
	case 8:
		if (machine->hostConfirmed == 0) {
			if (*event == 8)
				tx_timer_deactivate(&machine->confirmTimer);
			machine->awakeReportDue = 1;
		}
		wlanPsSetPowerState(1);
		break;
	case 11:
		if (!wlanPsAssociated(first))
			break;
		wlanPsActivity(connection);
		wlanPsSendNull(first, 0, 0, WLAN_NULL_KIND_SLEEP);
		wlanPsRestartTimer(&machine->nullTimer, WLAN_PS_NULL_TICKS);
		break;
	case 17:
		first = wlanConnectionFirst(0);
		if (first != NULL)
			wlanPostConnectionEvent(first, WLAN_EVENT_HOST_PS_PEER);
		return NULL;
	case WLAN_PS_EVENT_TRAFFIC:
		osaCancelTimer(sleep->activityTimer);
		sleep->activityTimer = 0;
		sleep->recentActivity = 0;
		osaCancelTimer(sleep->sleepTimer);
		sleep->sleepTimer = 0;
		if (machine->hostConfirmed == 1 && machine->state == &machine->awake)
			romHsmTransition(machine, &machine->confirm);
		machine->sleepBlocked = 1;
		return NULL;
	case WLAN_PS_EVENT_IEEE_ENTER:
		machine->resume = &machine->on;
		romHsmTransition(machine, &machine->ieeeNull);
		return NULL;
	case WLAN_PS_EVENT_IEEE_EXIT:
		machine->resume = &machine->on;
		romHsmTransition(machine, &machine->ieeeExit);
		return NULL;
	case 43:
	case 44:
	case 45:
		mdevRomQueryState(&wlanStaPs, *event, NULL);
		return NULL;
	default:
		return event;
	}
	romHsmTransition(machine, &machine->awake);
	return NULL;
}

/* 0x13978: a broadcast null data frame whose duration covers the sleep. */
void wlanPsAnnounceSleep(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanReplyFrame *request;
	WlanWirelessFrame *frame;
	OsaBuffer *buffer;
	uint8_t *data;
	uint32_t *txControl;
	unsigned int duration;
	unsigned int ring;
	int result;

	buffer = wlanMgmtAllocFrame(connection, WLAN_PS_ANNOUNCE_SIZE);
	if (buffer == NULL)
		return;
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	data = buffer->data;
	request = (struct WlanReplyFrame *)data;
	buffer->dataOffset = 28;
	__aeabi_memclr4(data, offsetof(struct WlanReplyFrame, frameLength));
	request->descriptor.header.wire.packetLength[0] = wlanTxControlRate(connection);
	txControl = (uint32_t *)&request->descriptor.priority;	/* bytes 12-15: the transmit control word */
	*txControl = (*txControl & ~0x1fu) | request->descriptor.header.wire.packetLength[0] | 0x10;
	request->frame = frame->bodyLength;
	request->descriptor.control = 0x1500;
	TX_DATA_TAG(data) = 20;
	wlanInitMgmtHeader(frame, 2, 4, wlanPsBroadcast, wlanDeviceAddress->address, wlanDeviceAddress->address);
	frame->frameControl[1] |= 2;
	duration = info->sleep.sleepDuration;
	frame->duration[0] = (uint8_t)duration;
	frame->duration[1] = (uint8_t)(duration >> 8);
	if (info->sleep.mode == WLAN_PS_SLEEP_DTIM) {
		TX_DATA_RING(data) = 12;
		wlanTxRequestBuild(connection, data);
		result = wlanTxWindowResubmit(buffer);
	} else {
		ring = 0;
		if (info->qosEnabled == 1 || wlanWmmEnabled())
			ring = wlanAccessCategoryToQueue(0);
		TX_DATA_RING(data) = ring;
		result = wlanTxSubmit(buffer);
	}
	if (result == 0)
		wlanCompletePacket(buffer, 0);
}

/* 0x13a48: is the next DTIM beacon too close (or the sleep before it too short)? */
int wlanPsDtimTooClose(struct WlanPsSleep *sleep, uint64_t now)
{
	struct WlanConnection *first = wlanConnectionFirst(0);
	uint64_t interval;
	uint64_t elapsed;
	uint64_t position;
	uint64_t left;
	unsigned int dtimPeriod;
	uint32_t dtimInterval;
	int beacons;

	if (!wlanPsAssociated(first))
		return 0;
	interval = (int)(first->beaconPeriod << 10);
	if (now >= sleep->tbttReference)
		elapsed = now - sleep->tbttReference;
	else
		elapsed = ~(sleep->tbttReference - now);
	position = sleep->tsfReference + elapsed;
	beacons = (uint32_t)(elapsed / interval);
	dtimPeriod = first->dtimPeriod;
	beacons = (int)sleep->dtimCount - (int)((uint32_t)beacons % dtimPeriod);
	if (beacons <= 0)
		beacons += dtimPeriod;
	left = (uint64_t)((int64_t)beacons * (int64_t)interval) - position % interval;
	dtimInterval = (uint32_t)(interval * dtimPeriod);
	if (left <= WLAN_PS_GUARD || left >= dtimInterval - WLAN_PS_GUARD)
		return 1;
	left -= WLAN_PS_GUARD;
	if (left >= sleep->sleepDuration)
		return 0;
	sleep->sleepDuration = (uint32_t)left;
	if ((uint32_t)left <= sleepWakeLatency + WLAN_PS_MIN_SLEEP)
		return 1;
	return 0;
}

/* 0x13b4c: may the device sleep now; the sleep is shortened to end before the next beacon. */
int wlanPsMachineMaySleep(struct WlanPsMachine *machine, uint64_t now)
{
	struct WlanConnection *connection = machine->connection;
	struct WlanPsSleep *sleep = wlanPsSleepOf(connection);
	uint32_t interval = sleep->beaconInterval;
	uint32_t left = interval - (uint32_t)(now % interval);
	uint32_t margin = MAC_WAKE_LEAD + 250;

	if (left > margin && left < sleep->sleepDuration + margin)
		sleep->sleepDuration = left - margin;
	if (machine->sleepBlocked == 1)
		return 1;
	if (left < sleep->sleepDuration + margin || sleep->sleepAllowed != 1 ||
	    sleep->sleepDuration < sleepWakeLatency + WLAN_PS_MIN_SLEEP ||
	    wlanBssQueuedFrames(connection) != 0 || sleep->recentActivity != 0 ||
	    sleep->sleepTimer != 0 || wlanCryptoCount(0) != 0 || wlanCryptoCount(0) != 0 ||
	    mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PENDING, 0) != 0 || wlanDmaRing3HasRoom() != 0 ||
	    wlanPsDtimTooClose(sleep, now) != 0 || !macTxPoolsIdle())
		return 0;
	return 1;
}

/* 0x13c06 */
const int32_t *wlanPsMachineConfirm(struct WlanPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanPsSleep *sleep = &info->sleep;
	struct WlanConnection *first = wlanConnectionFirst(0);
	uint64_t now;

	switch (*event) {
	case 16:
		if (machine->nullRetries < 2 && machine->txPending == 0 && machine->nullFailed == 0 &&
		    macTxPoolsIdle() && wlanPsAssociated(first)) {
			wlanPsSendNull(first, 1, 0, WLAN_NULL_KIND_SLEEP);
			machine->nullRetries++;
		}
		return NULL;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		now = romGetTimerTimestamp();
		machine->announced = 0;
		machine->nullAcked = 0;
		machine->txPending = 0;
		machine->nullRetries = 0;
		macTxSetPowerSave(1);
		sleep->sleepDuration = sleep->maxSleep;
		if (!wlanApPsEnabled() || connection->interfaceType != 2 ||
		    !wlanPsMachineMaySleep(machine, now))
			goto awake;
		sleep->sleptAt = romGetTimerTimestamp();
		sleep->wakeAt = sleep->sleptAt + sleep->sleepDuration;
		if (wlanPsAssociated(first) && wlanTxPowerSaveCount != 0) {
			wlanPsSendNull(first, 1, 0, WLAN_NULL_KIND_SLEEP);
			wlanPsRestartTimer(&machine->nullTimer, WLAN_PS_NULL_TICKS);
		} else {
			machine->nullAcked = 1;
		}
		if (machine->sleepReportDue == 1) {
			machine->sleepReportDue = 0;
			machine->hostConfirmed = 0;
			wlanPostConnectionEvent(machine->connection, WLAN_EVENT_HOST_PS_SLEEP);
			wlanPsRestartTimer(&machine->confirmTimer, WLAN_PS_CONFIRM_TICKS);
		}
		if (machine->sleepBlocked != 0 || sleep->mode == WLAN_PS_SLEEP_DTIM ||
		    (sleep->control & 1) == 0 || sleep->sleepDuration > WLAN_PS_SLEEP_DURATION_MAX ||
		    info->stationCount == 0)
			machine->announced = 1;
		else
			wlanPsAnnounceSleep(machine->connection);
		if (machine->hostConfirmed != 1 || machine->announced != 1 || machine->nullAcked != 1)
			return NULL;
		tx_timer_deactivate(&machine->confirmTimer);
		goto sleep;
	case 14:
		tx_timer_deactivate(&machine->nullTimer);
		machine->nullAcked = 1;
		wlanTxPowerSaveCount = 0;
		goto acked;
	case 15:
		machine->txPending = 1;
		if (machine->nullFailed != 0)
			return NULL;
		tx_timer_deactivate(&machine->confirmTimer);
		if (machine->sleepReportDue == 0)
			machine->awakeReportDue = 1;
		goto awake;
	case 17:
		machine->nullFailed = 1;
		first = wlanConnectionFirst(0);
		if (first != NULL)
			wlanPostConnectionEvent(first, WLAN_EVENT_HOST_PS_PEER);
		machine->nullAcked = 1;
		tx_timer_deactivate(&machine->nullTimer);
		goto acked;
	case 29:
		machine->announced = 1;
		if (machine->hostConfirmed != 1 || machine->nullAcked != 1)
			return NULL;
		goto sleep;
	case 30:
		tx_timer_deactivate(&machine->confirmTimer);
		if (machine->sleepReportDue == 0)
			machine->awakeReportDue = 1;
		goto awake;
	default:
		return event;
	}

acked:
	if (machine->hostConfirmed != 1 || machine->announced != 1)
		return NULL;
sleep:
	romHsmTransition(machine, &machine->sleep);
	return NULL;
awake:
	romHsmTransition(machine, &machine->awake);
	return NULL;
}

/* 0x13e88 */
const int32_t *wlanPsMachineSleep(struct WlanPsMachine *machine, const int32_t *event)
{
	struct WlanPsSleep *sleep = wlanPsSleepOf(machine->connection);
	uint32_t margin = (uint16_t)(sleepWakeLatency + 345);

	if (*event == ROM_HSM_EXIT)
		return NULL;
	if (*event != ROM_HSM_ENTRY)
		return event;
	sleepCalibrationUpdate();
	osaPauseTimers();
	if (!wlanApPsEnabled() || machine->sleepBlocked != 0 ||
	    sleepEnter(&sleep->wakeAt, margin, 1000, sleep->sleepDuration, 1) == 0) {
		osaResumeTimers();
		machine->sleepBlocked = 0;
		romHsmTransition(machine, &machine->awake);
	}
	return NULL;
}

/* 0x13efe */
const int32_t *wlanPsMachineAwake(struct WlanPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;
	struct WlanPsSleep *sleep = wlanPsSleepOf(connection);
	struct WlanConnection *first = wlanConnectionFirst(0);

	switch (*event) {
	case 15:
		machine->txPending = 1;
		return NULL;
	case 16:
		if (machine->nullRetries < 2 && machine->txPending == 0 && machine->nullFailed == 0 &&
		    macTxPoolsIdle() && wlanPsAssociated(first)) {
			wlanPsActivity(connection);
			wlanPsSendNull(first, 0, 0, WLAN_NULL_KIND_SLEEP);
			machine->nullRetries++;
		}
		return NULL;
	case 18:
		romHsmTransition(machine, &machine->confirm);
		return NULL;
	case 19:
		macTxPostEvent(1);
		return NULL;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		macTxSetPowerSave(0);
		wlanPsMachineReportAwake(machine);
		macTxWakeup();
		if (machine->eventPending) {
			wlanRaiseEvent(17);
			machine->eventPending = 0;
		}
		machine->txPending = 0;
		machine->nullRetries = 0;
		if (sleep->mode == WLAN_PS_SLEEP_INACTIVITY && sleep->recentActivity == 0)
			wlanPsStartSleepTimer(connection);
		macTxPostEvent(5);
		return NULL;
	case 13:
		tx_timer_deactivate(&machine->nullTimer);
		return NULL;
	}
	return event;
}

/* 0x13ffc */
const int32_t *wlanPsMachineIeee(struct WlanPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *connection = machine->connection;

	switch (*event) {
	case 1:
		machine->resume = &machine->on;
		wlanPsHostCmdRespond(connection);
		return NULL;
	case 2:
		machine->resume = &machine->off;
		wlanPsHostCmdRespond(connection);
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

/* 0x14038: PM=1 null frames until the access point acknowledges, at most three more. */
const int32_t *wlanPsMachineIeeeNull(struct WlanPsMachine *machine, const int32_t *event)
{
	struct WlanConnection *first = wlanConnectionFirst(0);
	unsigned int tries;

	switch (*event) {
	case 43:
		if (machine->resume == &machine->off && machine->ieeeState == 3) {
			macTxSetPowerSave(0);
			wlanPsIeeeFlag = 0;
			wlanTxPowerSaveCount = 0;
			machine->ieeeTries = 0;
			machine->ieeeState = 2;
			if (wlanPsAssociated(first))
				wlanPsSendNull(first, 0, 1, WLAN_NULL_KIND_IEEE);
		}
		macTxReleaseWakeBits(&machine->wakeBits);
		wlanPsSetPowerState(1);
		romHsmTransition(machine, machine->resume);
		return NULL;
	case 44:
	case 45:
		wlanPsSetPowerState(1);
		machine->ieeeState = 0;
		macTxSetPowerSave(0);
		macTxReleaseWakeBits(&machine->wakeBits);
		machine->eventPending = 0;
		romHsmTransition(machine, machine->resume);
		return NULL;
	case ROM_HSM_EXIT:
		goto done;
	case ROM_HSM_ENTRY:
		wlanPsMachineStopTimers(machine);
		machine->ieeeState = 0;
		machine->ieeeTries = 0;
		machine->wakeBits = macTxAcquireWakeBit();
		if (!wlanPsAssociated(first) || macTxGetPowerSave() != 0)
			goto done;
		macTxSetPowerSave(1);
		machine->ieeeState = 1;
		wlanPsSendNull(first, 1, 0, WLAN_NULL_KIND_IEEE);
		return NULL;
	case 39:
		if (machine->ieeeState == 1)
			goto gaveUp;
		return NULL;
	case 40:
	case 47:
		if (machine->ieeeState == 1) {
			tries = machine->ieeeTries++;
			if (tries > 2)
				goto gaveUp;
			if (wlanPsAssociated(first))
				wlanPsSendNull(first, 1, 0, WLAN_NULL_KIND_IEEE);
		} else if (machine->ieeeState == 2) {
			tries = machine->ieeeTries++;
			if (tries > 2)
				goto gaveUp;
			if (wlanPsAssociated(first))
				wlanPsSendNull(first, 0, 0, WLAN_NULL_KIND_IEEE);
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

/* 0x141ac */
const int32_t *wlanPsMachineIeeeExit(struct WlanPsMachine *machine, const int32_t *event)
{
	switch (*event) {
	case 43:
		break;
	case 44:
		machine->ieeeState = 0;
		machine->eventPending = 0;
		break;
	case 45:
		machine->ieeeState = 0;
		macTxSetPowerSave(0);
		sleepCalibrationUpdate();
		sleepWake();
		machine->eventPending = 0;
		break;
	case ROM_HSM_EXIT:
		osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
		return NULL;
	case ROM_HSM_ENTRY:
		wlanPsMachineStopTimers(machine);
		macTxSetPowerSave(0);
		machine->ieeeTries = 0;
		osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
		return NULL;
	default:
		return event;
	}
	romHsmTransition(machine, machine->resume);
	return NULL;
}

/* 0x14228 */
UINT wlanPsMachineCreate(struct WlanConnectionInfo *info)
{
	struct WlanPsMachine *machine = &info->psMachine;

	wlanPsMachineInitStates(machine);
	romHsmStart(machine, &machine->off);
	tx_timer_create(&machine->confirmTimer, "SleepConfirmTmr", wlanPsSleepConfirmTimerCb, 0,
			WLAN_PS_CONFIRM_TICKS, 0, 0);
	return tx_timer_create(&machine->nullTimer, "AP_NullPktDoneTmr", wlanPsApNullPktDoneTimerCb,
			       0, WLAN_PS_NULL_TICKS, 0, 0);
}

/* 0x14274 */
int wlanPsMachineReset(struct WlanConnectionInfo *info)
{
	struct WlanPsMachine *machine = &info->psMachine;
	struct WlanPsSleep *sleep = &info->sleep;

	romHsmStart(machine, &machine->off);
	machine->awakeReportDue = 1;
	machine->hostConfirmed = 0;
	machine->announced = 0;
	machine->nullAcked = 0;
	machine->sleepReportDue = 1;
	machine->txPending = 0;
	machine->nullRetries = 0;
	machine->nullFailed = 0;
	wlanTxPowerSaveCount = 0;
	machine->resume = &machine->off;
	machine->ieeeState = 0;
	machine->ieeeTries = 0;
	machine->eventPending = 0;
	machine->wakeBits = 0;
	sleep->recentActivity = 0;
	sleep->sleepAllowed = 0;
	sleep->mode = WLAN_PS_SLEEP_INACTIVITY;
	sleep->wakeAt = 0;
	sleep->sleptAt = 0;
	sleep->sleepDuration = 17000;
	sleep->maxSleep = 17000;
	sleep->wakeLatency = 1300;
	sleep->control = 1;
	sleep->minSleep = 17000;
	sleep->inactivityTimeout = 200000;
	sleep->maxAwake = 2000;
	sleep->minAwake = 2000;
	sleep->beaconPeriod = 100;
	sleep->sleepTimer = 0;
	sleep->beaconInterval = 102400;
	sleep->activityTimer = 0;
	return 0;
}

/* 0x142f4: frames of subtype 4 are not passed on. */
int wlanPsRxFilter(const uint8_t *frame, uint8_t *forward)
{
	if ((((const WlanWirelessFrame *)frame)->frameControl[0] >> 4) == 4)
		*forward = 0;
	return 0;
}

/* 0x14306 */
void wlanPsMachineInitStates(struct WlanPsMachine *machine)
{
	romHsmStateInit(&machine->root, NULL, (RomHsmHandler)wlanPsMachineRoot);
	romHsmStateInit(&machine->off, &machine->root, (RomHsmHandler)wlanPsMachineOff);
	romHsmStateInit(&machine->on, &machine->root, (RomHsmHandler)wlanPsMachineOn);
	romHsmStateInit(&machine->ieee, &machine->root, (RomHsmHandler)wlanPsMachineIeee);
	romHsmStateInit(&machine->confirm, &machine->on, (RomHsmHandler)wlanPsMachineConfirm);
	romHsmStateInit(&machine->sleep, &machine->on, (RomHsmHandler)wlanPsMachineSleep);
	romHsmStateInit(&machine->awake, &machine->on, (RomHsmHandler)wlanPsMachineAwake);
	romHsmStateInit(&machine->ieeeNull, &machine->ieee, (RomHsmHandler)wlanPsMachineIeeeNull);
	romHsmStateInit(&machine->ieeeExit, &machine->ieee, (RomHsmHandler)wlanPsMachineIeeeExit);
}
