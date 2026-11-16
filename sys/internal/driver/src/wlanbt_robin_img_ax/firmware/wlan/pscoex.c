/*
 * The questions the power-save and Bluetooth-coexistence code ask each other: whether the
 * radio is shared with Bluetooth, whether the null-frame and PS-Poll triggers may go out, and the
 * state kept for them. The filename is inferred; no source filename survives.
 */
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/eventpower.h"
#include "robin/pspoll.h"
#include "robin/psctl.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/stapssm.h"
#include "robin/sys.h"

/* Not converted yet. */
int macMgmtParseContext(void);			/* 0x356b4 */
int wlanPsComputeHeader(void);			/* 0x3dfbe: the station's pending session */
void wlanTxRingsFlush(void);			/* 0x32134 */

/* MAC_REG(0x2fc): the bits the coexistence code sets and clears around a time-sharing window. */
#define MAC_COEX_TRIGGER_BITS		0x6f
#define MAC_COEX_TRIGGER_RETAINED	0x67
#define MAC_COEX_TRIGGER_BIT3		0x08
/* MAC_REG(0x708) bit 25: the MAC reports events while the triggers are held. */
#define MAC_EVENT_ENABLE_BT_HOLD	0x2000000

/* Why wlanPsEvaluate() is called. */
enum {
	WLAN_PS_EVALUATE_RESET,		/* wlanPsPollReset() */
	WLAN_PS_EVALUATE_END,		/* the connection ends: stop polling, release the hold */
	WLAN_PS_EVALUATE_UPDATE,	/* a Bluetooth profile or direction change */
	WLAN_PS_EVALUATE_WINDOW,	/* a time-sharing window opens */
	WLAN_PS_EVALUATE_TRIGGER,	/* the triggers resume if nothing holds them */
	WLAN_PS_EVALUATE_NONE,
};

uint8_t wlanPsPollMode __attribute__((section(".itcm.rw"))) = 1;	/* 0x54f24: 1: the radio is shared with Bluetooth */
int wlanPsBtHold __attribute__((section(".itcm.rw")));		/* 0x54f28: the null frame and PS-Poll triggers wait for Bluetooth */

/* Stubs of the automatic frequency control (host command 60) and of the hook that follows it in macMgmtReleaseContext. */
void hostCmd003c(int offset, int frequency)
{
}

int wlanFrequencyTrackingStub(void)
{
	return 0;
}

/* PS-Poll is timed by the MAC's timer. */
int wlanPsPollUsesMacTimer(void)
{
	return 1;
}

/* The triggers are held for Bluetooth and Bluetooth is not using the medium. */
int wlanPsTriggerEnabled(void)
{
	return wlanPsBtHold != 0 && wlanCoexActive() == 0;
}

/* Bluetooth is sharing the radio: a profile that polls or a profile of bit 3 is on, and the link is not the second one. */
int wlanCoexBtNeedsSharing(void)
{
	return wlanPsPollMode == 1 &&
	       (btCoexProfilePollable() != 0 || btCoexProfileBit3() != 0) &&
	       macMgmtParseContext() != 2;
}

/* The triggers are to be held for Bluetooth: its profile of bit 1 is on and it is not using the medium now. */
int wlanPsBtHoldWanted(void)
{
	return wlanCoexBtNeedsSharing() && btCoexProfileBit1() != 0 && wlanCoexActive() == 0;
}

/* PS-Poll is forced by the sharing and power save being off. */
int wlanCoexPsPollForced(void)
{
	return wlanCoexBtNeedsSharing() && btCoexProfileBit1() != 0 && wlanPsDisabled();
}

/* The radio must stay awake: a pending session while Bluetooth shares it, a service period or a hold request. */
int wlanPsSleepBlocked(void)
{
	int result;

	if (wlanCoexBtNeedsSharing() && wlanPsComputeHeader() == 0)
		return 1;
	if (wlanStaPsServicePeriod() != 0)
		return 1;
	result = sleepHoldRequested();
	if (result != 0)
		return 1;
	return result;
}

/* A trigger may go out: Bluetooth is not using the medium, or it lets the transmitter go. */
int wlanPsTriggerWanted(void)
{
	return wlanCoexActive() == 0 || wlanCoexTxAllowed();
}

/* The end of Bluetooth's window has been asked for. */
int wlanPsCoexWindowEnd(void)
{
	return wlanCoexWindowEndRequested();
}

uint32_t wlanPsTriggerMode(void)
{
	return wlanPsBtHold;
}

void wlanPsSetBtHold(int hold)
{
	wlanPsBtHold = hold;
}

uint8_t wlanPsPollEnabled(void)
{
	return wlanPsPollMode;
}

void wlanPsSetPollEnabled(uint8_t mode)
{
	wlanPsPollMode = mode;
}

/* Bluetooth is off, or it is not in a mode that takes the medium. */
int wlanPsBtShareAllowed(void)
{
	return wlanCoexActive() == 0 || wlanCoexBtMode() == 0;
}

/* The hold ends and the frames the rings kept back go out. */
void wlanPsBeginBtHold(void)
{
	wlanPsBtHold = 0;
	wlanTxRingsFlush();
}

/* A trigger went out: Bluetooth's transmit window opens, or the PS-Poll is kicked. */
void wlanPsTriggerSent(void)
{
	if (wlanCoexActive() != 0)
		wlanCoexReleaseTx();
	else
		wlanPsPollKick(0);
}

/* Nothing is in flight: no ring has a frame, no PS-Poll is out and the coexistence deferred nothing. */
int wlanPsIdle(void)
{
	return wlanTxRingsPending() == 0 && wlanPsPollBusy() == 0 && wlanCoexDeferred() == 0;
}

/* The two hooks of wlanPsEvaluate() (0x3d068, 0x3d06a): the ROM's packet-hold callbacks are empty here. */
void wlanPsResetPacket(struct WlanConnection *connection)
{
}

void wlanPsSyncPacket(struct WlanConnection *connection)
{
}

/* The MAC stops reporting events for the triggers, and the 0x6f trigger bits go out. */
static inline void wlanPsReleaseMacTriggers(void)
{
	MAC_REG(0x2fc) &= ~MAC_COEX_TRIGGER_BITS;
	MAC_REG(0x708) &= ~MAC_EVENT_ENABLE_BT_HOLD;
}

/* The station's time-sharing state follows an event (WLAN_PS_EVALUATE_*); always answers 1 but the update case. */
int wlanPsEvaluate(struct WlanConnection *connection, int reason)
{
	if (macMgmtParseContext() == 2)
		return 1;
	switch (reason) {
	case WLAN_PS_EVALUATE_RESET:
		wlanPsPollReset();
		return 1;
	case WLAN_PS_EVALUATE_END:
		wlanPsPollStop();
		wlanPsSyncPacket(connection);
		wlanCoexHandleEvent(connection, WLAN_EVENT_LINK_DOWN);
		wlanPsBeginBtHold();
		wlanPsReleaseMacTriggers();
		return 1;
	case WLAN_PS_EVALUATE_UPDATE:
		if (wlanCoexHandleEvent(connection, WLAN_EVENT_COEX_UPDATE) == 0)
			return 0;
		break;
	case WLAN_PS_EVALUATE_WINDOW:
		wlanCoexHandleEvent(connection, WLAN_EVENT_COEX_WINDOW);
		break;
	case WLAN_PS_EVALUATE_TRIGGER:
		if (wlanPsTriggerEnabled())
			wlanPsBeginBtHold();
		return 1;
	default:
		return 1;
	}
	if (wlanPsBtHoldWanted()) {
		wlanPsBtHold = 1;
		wlanPsResetPacket(connection);
		wlanPsPollUpdateTiming();
		if (wlanPsLegacyPollMode()) {
			MAC_REG(0x2fc) |= MAC_COEX_TRIGGER_BIT3;
			MAC_REG(0x2fc) &= ~MAC_COEX_TRIGGER_RETAINED;
		} else if (wlanCoexUseP2pSleepMode()) {
			MAC_REG(0x2fc) |= MAC_COEX_TRIGGER_BITS;
		}
		MAC_REG(0x708) |= MAC_EVENT_ENABLE_BT_HOLD;
		if (!wlanPsLegacyPollMode()) {
			wlanPsTriggerSent();
			return 1;
		}
		if (!wlanCoexUseStationMode())
			wlanPsBeginBtHold();
		return 1;
	}
	if (wlanCoexActive() == 0) {
		wlanPsReleaseMacTriggers();
		wlanCoexProgramMacTiming();
		wlanPsBeginBtHold();
		if (wlanStaPs.state == &wlanStaPs.sleep)
			wlanRaiseEvent(WLAN_EVENT_COEX_SLEEP_READY);
		return 1;
	}
	wlanPsReleaseMacTriggers();
	wlanCoexProgramMacTiming();
	wlanPsTriggerSent();
	return 1;
}

/* The profile asks for no polling and no window: Bluetooth does not need the medium on a schedule. */
static int btCoexProfileUnscheduled(uint32_t profile)
{
	return ((profile & BT_COEX_PROFILE_POLLING) == 0 || (profile & BT_COEX_PROFILE_NO_POLL) != 0) &&
	       (profile & (BT_COEX_PROFILE_WINDOW | BT_COEX_PROFILE_SLAVE_ONLY)) == 0;
}

/* The host changed the Bluetooth profile while the station shares the radio and sleeps. */
void btCoexProfileChanged(uint32_t previous, uint32_t profile)
{
	struct WlanConnection *connection = wlanConnectionByRole(0);
	UINT posture = sysIrqDisable();
	int wasUnscheduled = btCoexProfileUnscheduled(previous & 0xffff);
	int isUnscheduled = btCoexProfileUnscheduled(profile & 0xffff);

	if (connection != NULL && wlanPsPollMode == 1 && macMgmtParseContext() != 2 &&
	    wlanStaPs.state == &wlanStaPs.sleep) {
		if (wasUnscheduled && !isUnscheduled)
			wlanRaiseEvent(WLAN_EVENT_COEX_PROFILE_CHANGED);
		else if (((previous ^ profile) & BT_COEX_PROFILE_SLAVE_ONLY) != 0 ||
			 (!wasUnscheduled && isUnscheduled))
			wlanPsEvaluate(connection, WLAN_PS_EVALUATE_UPDATE);
	}
	sysIrqRestore(posture);
}

/*
 * Smooths how long the station stayed awake: the mean and its deviation move by a third
 * of the distance to each new sample (the span from start to now, capped).
 */
int wlanCoexUpdateEstimate(uint64_t start, uint64_t now, uint32_t maxSample, int32_t *mean,
			   uint32_t maxDeviation, int32_t *deviation)
{
	int32_t sample = (int32_t)(now - start);
	int32_t smoothed;
	int32_t high;
	int32_t low;
	uint32_t spread;

	if (sample <= 0)
		return 0;
	if ((uint32_t)sample > maxSample)
		sample = maxSample;
	smoothed = (*mean * 2 + sample) / 3;
	*mean = smoothed;
	high = smoothed < sample ? sample : smoothed;
	low = smoothed > sample ? sample : smoothed;
	spread = high - low;
	if (spread > maxDeviation)
		spread = maxDeviation;
	*deviation = (*deviation * 2 + spread) / 3;
	return 1;
}

/* Pending-event mode: a window event for the control task, or the poll timer starts. */
int sysDispatchPendingEventMode(uint32_t mode)
{
	if (mode != 0)
		return wlanRaiseEvent(WLAN_EVENT_COEX_WINDOW);
	return wlanCoexStartPoll();
}
