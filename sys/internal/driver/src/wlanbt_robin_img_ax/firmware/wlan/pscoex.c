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

/* Not converted yet. */
int macMgmtParseContext(void);			/* 0x356b4 */
int wlanPsComputeHeader(void);			/* 0x3dfbe: the station's pending session */
void wlanTxRingsFlush(void);			/* 0x32134 */

uint8_t wlanPsPollMode __attribute__((section(".itcm.rw")));	/* 0x54f24: 1: the radio is shared with Bluetooth */
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
