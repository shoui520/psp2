/*
 * The host's sleep phase (0x17144..0x1760c): when the MAC has been idle for
 * wlanPsPhaseDelay seconds it announces the sleep to the host (event
 * 0x2000) and waits for the host's reply; the host's traffic and Bluetooth
 * HCI traffic end the phase. Phases (wlanConnectionPhase): 0 off, 1 the
 * reply is due without a countdown, 2 counting down, 3 announced, 4 the
 * reply is due after a countdown, 5 the host sleeps, 6 suspended. The names
 * are inferred from use; the filename is inferred, no source filename
 * survives.
 */
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/txhold.h"
#include "robin/conn.h"
#include "robin/dmaring.h"
#include "robin/hostcmd.h"
#include "robin/hostps.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/psctl.h"
#include "robin/pssm.h"
#include "robin/rom.h"
#include "robin/slot.h"
#include "robin/stapssm.h"
#include "robin/sys.h"
#include "robin/threadx.h"
#include "robin/wlantask.h"

#define SDIO_IOCTL_QUEUE_DRAINED	0x1e

/* wlanConnectionPhase */
#define WLAN_PHASE_OFF			0
#define WLAN_PHASE_REPLY_DUE		1	/* the host's reply ends the phase */
#define WLAN_PHASE_COUNTDOWN		2	/* wlanPendingTimer runs */
#define WLAN_PHASE_ANNOUNCED		3	/* the sleep was announced (event 0x2000) */
#define WLAN_PHASE_REPLY_COUNTDOWN	4	/* the host's reply restarts the countdown */
#define WLAN_PHASE_HOST_ASLEEP		5
#define WLAN_PHASE_SUSPENDED		6

/* Events to the control task (wlanPostConnectionEvent()). */
#define WLAN_EVENT_HOST_TRAFFIC		0x00040000

/* Control-task events (wlanRaiseEvent()). */
#define WLAN_PS_EVENT_HOST_MODE		1
#define WLAN_PS_EVENT_SLEEP_CONFIRMED	9
#define WLAN_PS_EVENT_CONFIRM_TIMEOUT	10
#define WLAN_PS_EVENT_POLL_DUE		21
#define WLAN_PS_EVENT_HOST_WAKE		26

uint8_t wlanConnectionPhase __attribute__((section(".itcm.rw")));		/* 0x54b70 */
uint8_t wlanConnectionStage __attribute__((section(".itcm.rw")));		/* 0x54b71 */
uint16_t wlanPsPhaseDelay __attribute__((section(".itcm.rw")));		/* 0x54b72: s */
uint16_t wlanPsPollWindow __attribute__((section(".itcm.rw")));		/* 0x54b76: ms */
uint32_t wlanPsHostActive __attribute__((section(".itcm.rw")));		/* 0x54b78 */
uint32_t wlanPendingTimer __attribute__((section(".itcm.rw")));		/* 0x54b7c */
uint32_t wlanPsHostModeTimer __attribute__((section(".itcm.rw")));		/* 0x54b80 */
uint16_t wlanPsSleepSequence __attribute__((section(".itcm.rw")));		/* 0x54b84 */

/* Not converted yet; provisional names. */
extern uint8_t btHciWlanAsleep;				/* 0x04000c90 */
struct WlanConnection *p2pDirectProcessEntry(unsigned int interfaceType);	/* 0x41250 */
int hostCmdIsHostSleepConfigure(const uint8_t *packet);		/* 0x2c972 */
int hostCmdIsHostSleepActivate(const uint8_t *packet);		/* 0x2c9a6 */

/* 0x17144: the host sent traffic during its session. */
int wlanPsNoteHostActive(OsaBuffer *buffer)
{
	if (wlanSessionId != 0)
		wlanPsHostActive = 1;
	return 0;
}

/* 0x17156 */
uint32_t wlanPsHostIsActive(void)
{
	return wlanPsHostActive;
}

/* 0x1715c: the host may enter its power-save mode (event 1). */
void wlanPsHostModeTimerExpired(uint32_t timer, uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;
	struct WlanConnection *peer;
	UINT posture;

	peer = p2pDirectProcessEntry(WLAN_IF_STATION);
	if (peer == NULL)
		peer = wlanConnectionFirst(WLAN_IF_STATION);
	if (connection->mode == 3 || connection->interfaceType == WLAN_IF_IBSS) {
		if (!(connection->flags2 & 0x10))
			return;
		if (peer != NULL && peer->state == 7)
			return;
		if (connection->interfaceType == WLAN_IF_IBSS && macMgmtDispatchDescriptor() == 0)
			return;
	}
	if (connection->mode == 2 && (!wlanHasApRole() || connection->state != 7))
		return;
	if ((connection->flags2 & 0x10) &&
	    (connection->mode == 3 || connection->interfaceType == WLAN_IF_IBSS) &&
	    !(wlanSessionId & 0x10))
		return;
	posture = sysIrqDisable();
	if (wlanPsHostMode == 0) {
		wlanPsHostMode = 1;
		sysIrqRestore(posture);
		wlanRaiseEvent(WLAN_PS_EVENT_HOST_MODE);
		return;
	}
	sysIrqRestore(posture);
}

/* 0x171f2 */
void wlanPsCancelHostModeTimer(void)
{
	if (wlanPsHostModeTimer == 0)
		return;
	osaCancelTimer(wlanPsHostModeTimer);
	wlanPsHostModeTimer = 0;
}

/* 0x17206 */
void wlanPsStartHostModeTimer(struct WlanConnection *connection)
{
	if (wlanHasApRole() && ((wlanSessionId >> 8) & 3) == 0)
		return;
	wlanPsCancelHostModeTimer();
	osaCreateTimer(wlanPsHostModeTimerExpired, (uintptr_t)connection, wlanPsPollWindow * 1000,
		       &wlanPsHostModeTimer, 0);
}

/* 0x1723c: the countdown ended: announce the sleep, or wait while the MAC is busy. */
void wlanPsPhaseTimerExpired(uint32_t timer, uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;

	if (timer != wlanPendingTimer)
		return;
	wlanPendingTimer = 0;
	if (wlanConnectionsIdle()) {
		if (wlanConnectionPhase != WLAN_PHASE_COUNTDOWN)
			return;
		if (hostCmdCurrentCode() != 0 || wlanSlotHasCurrent()) {
			osaCreateTimer(wlanPsPhaseTimerExpired, argument, wlanPsPhaseDelay * 1000,
				       &wlanPendingTimer, 0);
			return;
		}
		wlanConnectionPhase = WLAN_PHASE_ANNOUNCED;
		wlanPostConnectionEvent(connection, WLAN_EVENT_HOST_PS_SLEEP);
		return;
	}
	if (wlanConnectionPhase == WLAN_PHASE_OFF)
		return;
	if (wlanConnectionPhase == WLAN_PHASE_HOST_ASLEEP)
		wlanPostConnectionEvent(wlanConnectionByRole(wlanHasApRole() ? 2 : 0),
					WLAN_EVENT_HOST_PS_AWAKE);
	wlanConnectionPhase = WLAN_PHASE_SUSPENDED;
}

/* 0x172b8 */
int wlanPsCancelPendingTimer(void)
{
	int result = wlanPendingTimer;

	if (wlanPendingTimer != 0) {
		osaCancelTimer(wlanPendingTimer);
		wlanPendingTimer = 0;
		return 0;
	}
	return result;
}

/* 0x172cc */
void wlanPsPhaseSuspend(void)
{
	UINT posture = sysIrqDisable();

	if (wlanConnectionPhase != WLAN_PHASE_OFF && wlanConnectionPhase != WLAN_PHASE_SUSPENDED) {
		wlanPsCancelPendingTimer();
		wlanConnectionPhase = WLAN_PHASE_SUSPENDED;
	}
	sysIrqRestore(posture);
}

/* 0x172f0: restarts the countdown. */
void wlanPsPhaseArm(struct WlanConnection *connection)
{
	UINT posture = sysIrqDisable();

	switch (wlanConnectionPhase) {
	case WLAN_PHASE_OFF:
	case WLAN_PHASE_REPLY_DUE:
	case WLAN_PHASE_SUSPENDED:
	case WLAN_PHASE_ANNOUNCED:
	case WLAN_PHASE_REPLY_COUNTDOWN:
	case WLAN_PHASE_HOST_ASLEEP:
		break;
	default:
		wlanPsCancelPendingTimer();
		osaCreateTimer(wlanPsPhaseTimerExpired, (uintptr_t)connection, wlanPsPhaseDelay * 1000,
			       &wlanPendingTimer, 0);
		break;
	}
	sysIrqRestore(posture);
}

/* 0x1733a */
void wlanPsPhaseResume(struct WlanConnection *connection)
{
	UINT posture = sysIrqDisable();

	if (wlanConnectionPhase == WLAN_PHASE_SUSPENDED) {
		wlanConnectionPhase = WLAN_PHASE_COUNTDOWN;
		wlanPsPhaseArm(connection);
	}
	sysIrqRestore(posture);
}

/* 0x1735e */
void wlanPsPhaseReset(void)
{
	UINT posture = sysIrqDisable();

	wlanPsCancelPendingTimer();
	if (wlanConnectionPhase == WLAN_PHASE_REPLY_COUNTDOWN || wlanConnectionPhase == WLAN_PHASE_ANNOUNCED)
		wlanConnectionPhase = WLAN_PHASE_REPLY_DUE;
	else
		wlanConnectionPhase = WLAN_PHASE_OFF;
	sysIrqRestore(posture);
}

/* 0x17386: the host's confirmation is not waited for: event 10 at once. */
void wlanPsConfirmNow(void)
{
	struct WlanConnectionInfo *info;
	struct WlanPsMachine *machine = NULL;

	if (wlanRoleOtherOnly()) {
		tx_timer_deactivate(&wlanStaPs.confirmTimer);
	} else if (wlanHasApRole()) {
		info = wlanConnectionInfo(wlanConnectionByRole(2));
		if (info != NULL)
			machine = &info->psMachine;
		tx_timer_deactivate(&machine->confirmTimer);
	} else {
		tx_timer_deactivate(&wlanIbssPsMachine.confirmTimer);
	}
	wlanRaiseEvent(WLAN_PS_EVENT_CONFIRM_TIMEOUT);
}

/* 0x173c6: the host's reply to the sleep announcement. */
int wlanPsHostSleepReply(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;

	wlanPsHostActive = 0;
	wlanPsClearHostEvent();
	if ((wlanSessionId != 0 && wlanPsGetOverride() == 0) ||
	    wlanConnectionPhase == WLAN_PHASE_REPLY_DUE || wlanConnectionPhase == WLAN_PHASE_REPLY_COUNTDOWN) {
		if (mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PENDING, 0) == 0 && !wlanDmaRing3HasRoom()) {
			if (wlanSessionId != 0 &&
			    (((connection->mode == 3 || connection->interfaceType == WLAN_IF_IBSS) &&
			      (connection->flags2 & 0x10)) ||
			     (connection->mode == 2 && wlanApPsEnabled()))) {
				wlanRaiseEvent(WLAN_PS_EVENT_SLEEP_CONFIRMED);
				btHciWlanAsleep = 1;
				return 0;
			}
			if (wlanConnectionPhase == WLAN_PHASE_REPLY_COUNTDOWN) {
				btHciWlanAsleep = 1;
				wlanConnectionPhase = WLAN_PHASE_HOST_ASLEEP;
				wlanPostConnectionEvent(connection, WLAN_EVENT_HOST_ASLEEP);
				return 0;
			}
			wlanConnectionPhase = WLAN_PHASE_OFF;
		} else {
			if (wlanSessionId != 0)
				wlanPsConfirmNow();
			if (wlanConnectionPhase == WLAN_PHASE_REPLY_COUNTDOWN)
				wlanConnectionPhase = WLAN_PHASE_COUNTDOWN;
			else if (wlanConnectionPhase == WLAN_PHASE_REPLY_DUE)
				wlanConnectionPhase = WLAN_PHASE_OFF;
			else
				return 0;
		}
	} else if (wlanConnectionPhase != WLAN_PHASE_OFF && wlanConnectionPhase != WLAN_PHASE_SUSPENDED &&
		   wlanConnectionsIdle()) {
		if (wlanConnectionPhase != WLAN_PHASE_COUNTDOWN || !wlanConnectionsIdle() ||
		    !wlanPsGetOverride())
			return 0;
	}
	wlanPostConnectionEvent(buffer->context, WLAN_EVENT_HOST_PS_AWAKE);
	return 0;
}

/* 0x174a4: the host session begins. */
int wlanPsHostSessionStart(OsaBuffer *buffer)
{
	if (wlanApPsEnabled())
		return 0;
	if (wlanSessionId != 0 && wlanPsAltModeActive != 0) {
		wlanConnectionStage = 1;
		return 0;
	}
	if ((wlanConnectionPhase == WLAN_PHASE_OFF || wlanConnectionPhase == WLAN_PHASE_SUSPENDED) &&
	    !wlanIbssPsEnabled())
		wlanPsArmIdleWakeTimer();
	return 0;
}

/* 0x174e0: a Bluetooth HCI packet for the host. */
int wlanPsHciTraffic(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	const uint8_t *packet = (const uint8_t *)buffer + buffer->dataOffset;

	if (hostCmdIsHostSleepConfigure(packet)) {
		if (wlanPsGetOverride() &&
		    (wlanConnectionPhase == WLAN_PHASE_OFF || wlanConnectionPhase == WLAN_PHASE_SUSPENDED))
			wlanPostConnectionEvent(connection, WLAN_EVENT_HOST_TRAFFIC);
		if ((wlanRoleOtherOnly() || wlanHasApRole()) && wlanPsEnabled())
			wlanPsPostEvent(WLAN_PS_EVENT_TRAFFIC);
	}
	if (hostCmdIsHostSleepActivate(packet))
		return mdevIoctl(wlanSdioDev, SDIO_IOCTL_QUEUE_DRAINED, 0);
	return 0;
}

/* 0x17572: the host can take events again: the PS-Poll and host wake-up owed. */
void wlanPsHostReady(void)
{
	if (wlanPsHostBusy != 0 || mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_PENDING, 0) != 0)
		return;
	if (wlanPsPollFlag) {
		wlanPsPollFlag = 0;
		wlanRaiseEvent(WLAN_PS_EVENT_POLL_DUE);
	} else if (wlanPsHostWakePending) {
		wlanPsHostWakePending = 0;
		wlanRaiseEvent(WLAN_PS_EVENT_HOST_WAKE);
	}
}

/* 0x175ac */
void wlanPsHostSleepEnd(void)
{
	if (wlanHostSleepEndOnWake == 0 || !wlanHostSleepArmed())
		return;
	wlanHostSleepCancel();
	wlanMefConfig.wakeSignalled = 0;
	wlanHostSleepEndOnWake = 0;
}

/* 0x175cc */
void wlanPsStageAdvance(void)
{
	if (wlanSessionId != 0 && wlanConnectionStage == 1)
		wlanConnectionStage = 2;
}

/* 0x175e2 */
void wlanPsStageConfirm(void)
{
	if (wlanConnectionStage == 2)
		wlanConnectionStage = 3;
}
