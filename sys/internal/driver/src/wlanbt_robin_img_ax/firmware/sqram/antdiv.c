/*
 * Antenna diversity (SQRAM 0xc000a290..0xc000a6a8). Each supervision of a
 * peer's rate (antDivRecord()) steps a small machine: after a quiet time it
 * measures the beacons' SNR and the rate window on the current antenna,
 * switches, measures again, and switches back (antDivRevert()) when the new
 * antenna did worse. The filename and the names are inferred.
 */
#include <stdint.h>
#include "robin/antdiv.h"
#include "robin/calibration.h"
#include "robin/conn.h"
#include "robin/event.h"
#include "robin/hostcmd.h"
#include "robin/mgmt.h"
#include "robin/ratectl.h"
#include "robin/ratesel.h"
#include "robin/rom.h"
#include "robin/signal.h"

/* The machine's states (antDivState.state); odd states only wait one step. */
enum {
	ANT_DIV_IDLE = 0,
	ANT_DIV_SETTLE = 1,
	ANT_DIV_MEASURE = 2,
	ANT_DIV_SWITCHED = 3,
	ANT_DIV_MEASURE_SWITCHED = 4,
	ANT_DIV_SWITCHED_BACK = 5,
	ANT_DIV_MEASURE_BACK = 6,
};

#define ANT_DIV_TICK			10	/* milliseconds per supervision */
#define ANT_DIV_IDLE_TIME		6000
#define ANT_DIV_IDLE_TIME_START		5000
#define ANT_DIV_SETTLE_TIME		1000
#define ANT_DIV_SCAN_TIME		20000
#define ANT_DIV_HOLD_OFF		2
#define ANT_DIV_FRAMES			100
#define ANT_DIV_PERCENT_MIN		20
#define ANT_DIV_PERCENT_NONE		100	/* no measurement yet */
#define ANT_DIV_SNR_CCK_MIN		6
#define ANT_DIV_SNR_OFDM_MIN		7
#define ANT_DIV_SNR_MARGIN		3
#define ANT_DIV_PERCENT_MARGIN		5
#define ANT_DIV_FAST_RATE		5	/* a rate index from which the OFDM SNR decides */
#define ANT_DIV_PHY_CCK			2	/* macMgmtParseContext(): only CCK rates in use */
#define ANT_DIV_PATH_BOTH		7	/* macGetEventPrimaryPathState(): both antennas */
#define ANT_DIV_HISTORY_RUNNING		6	/* rateControl.historyCursor while a measurement runs */

struct AntDivState {
	uint8_t state;
	uint8_t holdOff;		/* supervisions to skip after a switch */
	uint8_t reserved2[2];
	uint32_t settleTime;		/* milliseconds, up to 1000 */
	uint32_t scanTime;		/* milliseconds, up to 20000 */
	uint32_t idleTime;		/* milliseconds, up to 6000 */
};

/* What one antenna gave: the beacons' SNR and the rate window. */
struct AntDivSample {
	int16_t snrCck;
	int16_t snrOfdm;
	uint32_t frames;
	uint8_t percent;
	uint8_t reserved9[3];
};

static struct AntDivState antDivState __attribute__((section(".sqram1.rw")));		/* 0xc0011f24 */
static struct AntDivSample antDivPrevious __attribute__((section(".sqram1.bss")));	/* 0xc0032efc */
static struct AntDivSample antDivCurrent __attribute__((section(".sqram1.bss")));	/* 0xc0032f08 */

/* Puts both radio paths on the configured antenna, then waits two supervisions. */
__attribute__((noinline)) void antDivApply(void)
{
	hostCmd0020Set(0, wlanRadioConfig->secondaryAntenna);
	hostCmd0020Set(1, wlanRadioConfig->secondaryAntenna);
	antDivState.holdOff = ANT_DIV_HOLD_OFF;
}

/* Picks the other antenna for the antenna setup of the primary path. */
__attribute__((noinline)) void antDivSwitch(void)
{
	unsigned int antenna = 0;

	switch (macGetEventPrimaryPathState()) {
	case 7:
		if (wlanRadioConfig->primaryAntenna == 0)
			antenna = 1;
		else if (wlanRadioConfig->primaryAntenna == 1)
			antenna = 2;
		break;
	case 3:
		if (wlanRadioConfig->primaryAntenna == 0)
			antenna = 1;
		break;
	case 5:
		if (wlanRadioConfig->primaryAntenna == 0)
			antenna = 2;
		break;
	case 6:
		if (wlanRadioConfig->primaryAntenna == 1)
			antenna = 2;
		else if (wlanRadioConfig->primaryAntenna == 2)
			antenna = 1;
		break;
	}
	wlanRadioConfig->primaryAntenna = antenna;
	wlanRadioConfig->secondaryAntenna = antenna;
}

/* Keeps the measurement and goes back to the configured antenna. */
__attribute__((noinline)) void antDivRevert(void)
{
	antDivPrevious.snrCck = antDivCurrent.snrCck;
	antDivPrevious.snrOfdm = antDivCurrent.snrCck;
	antDivPrevious.percent = antDivCurrent.percent;
	antDivPrevious.frames = antDivCurrent.frames;
	antDivApply();
}

__attribute__((noinline)) void antDivToggle(void)
{
	antDivSwitch();
	antDivApply();
}

/* After a measurement on the other antenna: back again when it did worse, else switch on. */
__attribute__((noinline)) void antDivDecide(struct WlanConnection *connection)
{
	if (antDivCurrent.percent == ANT_DIV_PERCENT_NONE && antDivCurrent.frames != 0)
		goto toggle;
	if (wlanTxDataRate(connection, 0) >= ANT_DIV_FAST_RATE && antDivPrevious.snrOfdm != 0 &&
	    antDivCurrent.snrOfdm != 0 && antDivCurrent.snrOfdm > antDivPrevious.snrOfdm + ANT_DIV_SNR_MARGIN)
		goto revert;
	if (macMgmtParseContext() != ANT_DIV_PHY_CCK && antDivPrevious.snrCck != 0 &&
	    antDivCurrent.snrCck != 0 && antDivCurrent.snrCck > antDivPrevious.snrCck + ANT_DIV_SNR_MARGIN)
		goto revert;
	if (antDivCurrent.frames < ANT_DIV_FRAMES || antDivPrevious.frames < ANT_DIV_FRAMES ||
	    antDivCurrent.percent + ANT_DIV_PERCENT_MARGIN >= antDivPrevious.percent)
		goto toggle;
revert:
	antDivRevert();
	return;
toggle:
	antDivToggle();
}

__attribute__((noinline)) void antDivTick(void)
{
	if (antDivState.idleTime <= ANT_DIV_IDLE_TIME)
		antDivState.idleTime += ANT_DIV_TICK;
	if (antDivState.settleTime <= ANT_DIV_SETTLE_TIME)
		antDivState.settleTime += ANT_DIV_TICK;
	if (antDivState.scanTime <= ANT_DIV_SCAN_TIME)
		antDivState.scanTime += ANT_DIV_TICK;
}

__attribute__((noinline)) void antDivReset(void)
{
	antDivState.idleTime = ANT_DIV_IDLE_TIME_START;
	antDivCurrent.snrCck = 0;
	antDivCurrent.snrOfdm = 0;
	antDivCurrent.percent = ANT_DIV_PERCENT_NONE;
	antDivCurrent.frames = 0;
	antDivPrevious.snrCck = 0;
	antDivPrevious.snrOfdm = 0;
	antDivPrevious.percent = ANT_DIV_PERCENT_NONE;
	antDivPrevious.frames = 0;
	antDivState.state = ANT_DIV_IDLE;
}

/* Starts over; with diversity on both paths, the antennas are chosen again first. */
void antDivStart(void)
{
	unsigned int setup;

	antDivReset();
	setup = ((const struct WlanAntennaProfile *)__aeabi_uread4(&wlanAntennaProfile))->antennaSetup;
	if ((((setup >> 2) & 3) & 2) && (setup & 2) && wlanRadioConfig->primaryDiversity != 0 &&
	    wlanRadioConfig->secondaryDiversity != 0) {
		wlanAntennaSelectSecondary();
		wlanAntennaSelectPrimary();
	}
	antDivApply();
}

/* 0 while a switch is still settling (one supervision less each call). */
int antDivHoldOff(void)
{
	if (antDivState.holdOff != 0) {
		antDivState.holdOff--;
		return 0;
	}
	return 1;
}

/* Nonzero while the current antenna has nothing to measure yet. */
__attribute__((noinline)) int antDivNotReady(struct WlanConnection *connection, int busy)
{
	if (busy != 0 || antDivState.settleTime >= ANT_DIV_SETTLE_TIME)
		return 0;
	if (antDivCurrent.frames >= ANT_DIV_FRAMES && antDivCurrent.percent >= ANT_DIV_PERCENT_MIN)
		return 0;
	if (!wlanSignalFilterReady(connection, 0))
		return 1;
	if (antDivCurrent.snrCck != 0 || antDivCurrent.snrOfdm != 0)
		return 0;
	return 1;
}

/* Takes a measurement and steps the machine; returns the new state. */
__attribute__((noinline, noclone)) unsigned int antDivStep(struct WlanConnection *connection, unsigned int rate, uint32_t frames, int busy)
{
	unsigned int state;

	if (wlanRadioConfig->primaryDiversity == 0)
		return antDivState.state;
	if (wlanBeaconCckSeen(connection))
		antDivCurrent.snrCck = wlanBeaconSnrCck(connection);
	else
		antDivCurrent.snrCck = 0;
	if (wlanBeaconOfdmSeen(connection))
		antDivCurrent.snrOfdm = wlanBeaconSnrOfdm(connection);
	else
		antDivCurrent.snrOfdm = 0;
	antDivCurrent.percent = rate;
	antDivCurrent.frames = frames;

	switch (antDivState.state) {
	case ANT_DIV_IDLE:
		if (antDivState.idleTime < ANT_DIV_IDLE_TIME)
			break;
		state = ANT_DIV_SETTLE;
		antDivState.idleTime = 0;
		goto set;
	case ANT_DIV_SETTLE:
		state = ANT_DIV_MEASURE;
		goto set;
	case ANT_DIV_MEASURE:
		if (antDivNotReady(connection, busy))
			break;
		antDivState.settleTime = 0;
		if (antDivState.scanTime >= ANT_DIV_SCAN_TIME ||
		    (antDivCurrent.snrCck > ANT_DIV_SNR_CCK_MIN &&
		     antDivCurrent.snrCck - antDivPrevious.snrCck < -1) ||
		    (antDivCurrent.snrOfdm > ANT_DIV_SNR_OFDM_MIN &&
		     antDivCurrent.snrOfdm - antDivPrevious.snrOfdm < -1)) {
			antDivState.scanTime = 0;
			antDivSwitch();
			antDivState.state = ANT_DIV_SWITCHED;
		} else {
			antDivState.state = ANT_DIV_IDLE;
		}
		antDivPrevious.percent = antDivCurrent.percent;
		antDivPrevious.snrCck = antDivCurrent.snrCck;
		antDivPrevious.snrOfdm = antDivCurrent.snrOfdm;
		antDivPrevious.frames = antDivCurrent.frames;
		break;
	case ANT_DIV_SWITCHED:
		state = ANT_DIV_MEASURE_SWITCHED;
		goto set;
	case ANT_DIV_MEASURE_SWITCHED:
		if (antDivNotReady(connection, busy))
			break;
		antDivState.settleTime = 0;
		antDivDecide(connection);
		if (macGetEventPrimaryPathState() != ANT_DIV_PATH_BOTH)
			goto idle;
		antDivSwitch();
		state = ANT_DIV_SWITCHED_BACK;
		goto set;
	case ANT_DIV_SWITCHED_BACK:
		state = ANT_DIV_MEASURE_BACK;
		goto set;
	case ANT_DIV_MEASURE_BACK:
		if (antDivNotReady(connection, busy))
			break;
		antDivState.settleTime = 0;
		antDivDecide(connection);
idle:
		antDivState.state = ANT_DIV_IDLE;
		break;
	set:
		antDivState.state = state;
		break;
	}
	return antDivState.state;
}

/* Starts a measurement on a peer: its signal and rate statistics are taken again. */
__attribute__((noinline)) void antDivRestart(struct WlanConnection *connection)
{
	connection->beaconRssiLast = wlanBeaconRssi(connection);
	connection->noiseLast = wlanNoiseFloor(connection);
	antDivApply();
	if (wlanRateSelect(&connection->peerLink.rateControl) > 5)
		wlanPsClearBuffer(connection);
	else
		wlanPsVerifyBuffer(connection);
	wlanSignalFilterReset(connection);
	antDivState.state = antDivStep(connection, 0, 0, 0);
}

/* The diversity step of a peer's rate supervision: 0 when it is idle, 2 while it measures. */
unsigned int antDivRecord(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	unsigned int state;

	antDivTick();
	if (rc->historyCursor == 0) {
		if (!(connection->flags2 & WLAN_CONN2_ASSOCIATED))
			return 0;
		state = antDivStep(connection, 0, 0, 0);
		antDivState.state = state;
		if (state != ANT_DIV_SETTLE)
			goto done;
		{
			int value = 0;

			wlanRateHistoryRecord(rc, &value);
			rc->historyCursor = ANT_DIV_HISTORY_RUNNING;
		}
		goto restart;
	} else if (rc->historyCursor == ANT_DIV_HISTORY_RUNNING) {
		int percent;
		uint8_t level;

		if (wlanPsParseFrame(&rc->window, &percent, &level, 0) == 2)
			goto done;
		state = antDivStep(connection, (uint8_t)percent, wlanPsPrepareFrame(&rc->window), level);
		antDivState.state = state;
		if (state == ANT_DIV_SWITCHED || state == ANT_DIV_SWITCHED_BACK) {
restart:
			antDivRestart(connection);
		} else if (state == ANT_DIV_IDLE) {
			wlanPsRecordBuffer(connection, 0);
		}
	}
done:
	if (rc->historyCursor == 0)
		return 0;
	return 2;
}
