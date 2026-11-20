/*
 * The updater ("Ernie Downloader", boot cluster 1: flash 0x1000-0xbfff and 0x35400-0x3ffff):
 * the resident program an update never rewrites (the update patches carry those ranges
 * zeroed). It programs the application's flash blocks (FlProg, FlChksum), checks the image
 * (SHA-1), swaps the boot clusters, and answers Kermit's commands meanwhile.
 *
 * Its main loop runs the mode (updMode) and the events the timer, the inputs and the host's
 * commands post (updEventSet): each mode takes the events it handles (updEventTake) and
 * changes mode by updModeChange, which also refuses the changes the mode does not allow.
 * The updater starts either from its reset vector (cstart, main) or from the application
 * (the entry at 0x2000, which carries the host link over).
 */
#pragma interrupt INTTM00 tm00Interrupt
#pragma sfr
#pragma DI
#pragma EI

#include <string.h>
#include "chip.h"
#include "ernie.h"
#include "board.h"
#include "charge.h"
#include "config.h"
#include "keepram.h"
#include "nvs.h"
#include "param.h"
#include "serial.h"
#include "serial1.h"
#include "serial2.h"
#include "serial4.h"
#include "sleepfactor.h"
#include "tau.h"
#include "timer.h"
#include "version.h"
#include "watchdog.h"
#include "powerseq.h"
#include "updater.h"
#if FW_SINCE(0x0100060D)
#include "handshake.h"
#include "snvs.h"
#endif



void mainLoop(void);

void main(void)
{
	rtcInit();
	sleepFactorInit();
#if FW_SINCE(0x0100060D)
	hostInit();
#else
	hostLink.frameSize = 0x20;
	hostLink.config = 0;
#endif
	updReset = 1;
	memcpy(updVersion, APP_VERSION_INFO->version, 4);
	timeStampDigits(updBuild, APP_VERSION_INFO->timeStamp);
	boardMultiCnOutputOff();
	mainLoop();
}

int updaterInitErnieUpdMode(void)
{
	if (updReset) {
		updMode = 0;
		paramInit();
		watchdogSet(&configGet()->watchdog);
		watchdogStop();
		updInput.bits.multiCnReported = updInput.bits.multiCn = updInput.bits.multiCnLast =
			!P2.0 ^ 1;
	} else {
		updMode = 3;
		hostOpen();
		updaterDecryptInit();
#if FW_SINCE(0x0100060D)
		handshakeInit(0);
		snvsReset();
#endif
		handshakeSetUnlocked(HANDSHAKE_UNLOCKED);
		updInput.bits.multiCnReported = updInput.bits.multiCn = updInput.bits.multiCnLast =
			!P2.0;
	}
	return 0;
}

unsigned char updModeGet(void)
{
	return updMode;
}

/* the change to mode, where the mode allows it (some reset the system instead); 1 if refused */
unsigned char updModeChange(unsigned char mode)
{
	unsigned char refused;

	refused = 1;
	switch (updMode) {
	case 0:
		if (mode == 1)
			refused = 0;
		break;
	case 1:
		switch (mode) {
		case 2:
			refused = 0;
			break;
		case 3:
#if FW_SINCE(0x0100060D)
#if SERIAL2_ARM
			if ((unsigned char)chargeMayStart() && !(unsigned char)chargeAlarm0On() &&
			    !(serial2TestAreaUsed() == 0 && !configGet()->test.bits.startCheckOff &&
			      serial2InTime() != 0)) {
#else
			if ((unsigned char)chargeMayStart() && !(unsigned char)chargeAlarm0On()) {
#endif
				handshakeInit(0);
				watchdogSetup();
				snvsReset();
				updaterDecryptInit();
				powerResume();
				hostOpen();
				hostInit();
				refused = 0;
			}
#else
			if (chargeMayStart()) {
				handshakeInit();
				watchdogSetup();
				updaterDecryptInit();
				powerResume();
				hostOpen();
				refused = 0;
			}
#endif
			break;
		case 5:
			refused = 0;
			setResetCause(0x10);
			resetSystem();
			break;
		case 6:
			refused = 0;
			setResetCause(0x11);
			resetSystem();
			break;
		}
		break;
	case 2:
		switch (mode) {
		case 1:
			refused = 0;
			break;
		case 6:
			refused = 0;
			setResetCause(0x10);
			resetSystem();
			break;
		}
		break;
	case 3:
		switch (mode) {
		case 4:
			DI();
			refused = 0;
			break;
#if FW_SINCE(0x0100060D)
		case 2:
			boardMultiCnOutputOff();
			powerOff();
			watchdogStop();
			refused = 0;
			break;
		case 1:
			hostClose();
			boardMultiCnOutputOff();
			powerOff();
			watchdogStop();
			refused = 0;
			break;
#else
		case 2:
			powerOff();
			boardMultiCnOutputOff();
			watchdogStop();
			refused = 0;
			break;
		case 1:
			hostClose();
			powerOff();
			boardMultiCnOutputOff();
			watchdogStop();
			refused = 0;
			break;
#endif
		case 5:
			if (hostSlowPending() && hostLink.resetWait--) {
				updEventSet(0x1f);
				break;
			}
			hostClose();
			powerOff();
			refused = 0;
			setResetCause(0x10);
			resetSystem();
			break;
		case 6:
			if (hostSlowPending() && hostLink.resetWait--) {
				updEventSet(0x20);
				break;
			}
			hostClose();
			powerOff();
			refused = 0;
			setResetCause(0x11);
			resetSystem();
			break;
		}
		break;
	case 4:
		switch (mode) {
		case 5:
			hostClose();
			powerOff();
			refused = 0;
			setResetCause(0x10);
			resetSystem();
			break;
		case 6:
			hostClose();
			powerOff();
			refused = 0;
			setResetCause(0x11);
			resetSystem();
			break;
		}
		break;
	}
	if (!refused) {
		UPD_LOG(("[UPDMOD] change %x to %x\r\n", updMode, mode));
		updMode = mode;
		updEventTake(0x66);
		updEventTake(0x69);
		updEventTake(0x64);
		updEventTake(0x67);
		updEventTake(0x14);
		updEventTake(0x15);
#if BOARD_SERIAL4
		serial4Answer0205();
#endif
		serialAnswer0105();
	}
	return refused;
}

/* takes an event: 1 if it was set */
unsigned char updEventTake(unsigned char event)
{
	unsigned char set;

	set = 0;
	switch (event) {
	case 0x01:
		if (set = updEvents.bits.event01)
			updEvents.bits.event01 = 0;
		break;
	case 0x02:
		if (set = updEvents.bits.event02)
			updEvents.bits.event02 = 0;
		break;
	case 0x03:
		if (set = updEvents.bits.event03)
			updEvents.bits.event03 = 0;
		break;
	case 0x04:
		if (set = updEvents.bits.event04)
			updEvents.bits.event04 = 0;
		break;
	case 0x05:
		if (set = updEvents.bits.event05)
			updEvents.bits.event05 = 0;
		break;
	case 0x14:
		if (set = updEvents.bits.event14)
			updEvents.bits.event14 = 0;
		break;
	case 0x15:
		if (set = updEvents.bits.event15)
			updEvents.bits.event15 = 0;
		break;
	case 0x16:
		if (set = updEvents.bits.event16)
			updEvents.bits.event16 = 0;
		break;
	case 0x1e:
		if (set = updEvents.bits.event1e)
			updEvents.bits.event1e = 0;
		break;
	case 0x1f:
		if (set = updEvents.bits.event1f)
			updEvents.bits.event1f = 0;
		break;
	case 0x20:
		if (set = updEvents.bits.event20)
			updEvents.bits.event20 = 0;
		break;
	case 0x32:
		if (set = updEvents.bits.event32)
			updEvents.bits.event32 = 0;
		break;
	case 0x33:
		if (set = updEvents.bits.event33)
			updEvents.bits.event33 = 0;
		break;
	case 0x34:
		if (set = updEvents.bits.event34)
			updEvents.bits.event34 = 0;
		break;
	case 0x35:
		if (set = updEvents.bits.event35)
			updEvents.bits.event35 = 0;
		break;
	case 0x0a:
		if (set = updEvents.bits.event0a)
			updEvents.bits.event0a = 0;
		break;
	case 0x64:
		if (set = updEvents.bits.event64)
			updEvents.bits.event64 = 0;
		break;
	case 0x67:
		if (set = updEvents.bits.event67)
			updEvents.bits.event67 = 0;
		break;
	case 0x69:
		if (set = updEvents.bits.event69)
			updEvents.bits.event69 = 0;
		break;
	case 0x66:
		if (set = updEvents.bits.event66)
			updEvents.bits.event66 = 0;
		break;
	case 0x78:
		if (set = updEvents.bits.event78)
			updEvents.bits.event78 = 0;
		break;
	case 0x79:
		if (set = updEvents.bits.event79)
			updEvents.bits.event79 = 0;
		break;
	case 0x7a:
		if (set = updEvents.bits.event7a)
			updEvents.bits.event7a = 0;
		break;
	case 0x82:
		if (set = updEvents.bits.event82)
			updEvents.bits.event82 = 0;
		break;
	}
	return set;
}

/* sets an event: 1 if there is no such event */
unsigned char updEventSet(unsigned char event)
{
	unsigned char unknown;

	unknown = 0;
	switch (event) {
	case 0x01:
		updEvents.bits.event01 = 1;
		break;
	case 0x02:
		updEvents.bits.event02 = 1;
		break;
	case 0x03:
		updEvents.bits.event03 = 1;
		break;
	case 0x04:
		updEvents.bits.event04 = 1;
		break;
	case 0x05:
		updEvents.bits.event05 = 1;
		break;
	case 0x14:
		updEvents.bits.event14 = 1;
		break;
	case 0x15:
		updEvents.bits.event15 = 1;
		break;
	case 0x16:
		updEvents.bits.event16 = 1;
		break;
	case 0x1e:
		updEvents.bits.event1e = 1;
		break;
	case 0x1f:
		updEvents.bits.event1f = 1;
		break;
	case 0x20:
		updEvents.bits.event20 = 1;
		break;
	case 0x32:
		updEvents.bits.event32 = 1;
		break;
	case 0x33:
		updEvents.bits.event33 = 1;
		break;
	case 0x34:
		updEvents.bits.event34 = 1;
		break;
	case 0x35:
		updEvents.bits.event35 = 1;
		break;
	case 0x0a:
		updEvents.bits.event0a = 1;
		break;
	case 0x64:
		updEvents.bits.event64 = 1;
		break;
	case 0x67:
		updEvents.bits.event67 = 1;
		break;
	case 0x69:
		updEvents.bits.event69 = 1;
		break;
	case 0x66:
		updEvents.bits.event66 = 1;
		break;
	case 0x78:
		updEvents.bits.event78 = 1;
		break;
	case 0x79:
		updEvents.bits.event79 = 1;
		break;
	case 0x7a:
		updEvents.bits.event7a = 1;
		break;
	case 0x82:
		updEvents.bits.event82 = 1;
		break;
	default:
		unknown = 1;
		break;
	}
	return unknown;
}

/* buttonPoll's times from the settings' button times (at least 3), in polls of 4 ms */
void buttonInit(void)
{
	buttonShortTime = 2;
	buttonHoldTime1 = (configGet()->pressTime1 >= 4 ? configGet()->pressTime1 : 3) * 500 / 4;
	buttonHoldTime2 = (configGet()->pressTime2 >= 4 ? configGet()->pressTime2 : 3) * 500 / 4;
}

/* the buttons' hold times counted: what was reached at this poll (BUTTON_*), 0 if nothing */
unsigned char buttonPoll(void)
{
	unsigned char keys;
	unsigned char event;

	event = 0;
	keys = (0x0f & P5) | ((!P11.5 ? (unsigned char)0xef : (unsigned char)0xff) &
			      (!P4.6 ? (unsigned char)0xbf : (unsigned char)0xff) & 0xf0);
	if (keys & 0x10)
		buttonPowerHeld = 0;
	else
		buttonPowerHeld++;
	if (keys & 0x40)
		buttonPsHeld = 0;
	else
		buttonPsHeld++;
	if (buttonPsHeld == buttonHoldTime2)
		event = 6;
	if (buttonPowerHeld == buttonHoldTime1 && buttonPsHeld >= buttonHoldTime1)
		event = 2;
	if (buttonPsHeld == buttonHoldTime1 && buttonPowerHeld >= buttonHoldTime1)
		event = 2;
	if (buttonPowerHeld == buttonShortTime)
		event = 1;
	if (buttonPsHeld == buttonShortTime)
		event = 4;
	if (buttonPsHeld == buttonHoldTime1)
		event = 5;
	buttonPolls++;
	return event;
}

#if FW_SINCE(0x0100060D)
/* the multi connector (P2.0) followed: event 0x78, which reads what is there */
int inputPoll(void)
{
	updInput.bits.multiCnLast = updInput.bits.multiCn;
	updInput.bits.multiCn = !P2.0;
	if (updInput.bits.multiCn == updInput.bits.multiCnLast &&
	    updInput.bits.multiCn ^ updInput.bits.multiCnReported) {
		updInput.bits.multiCnReported = updInput.bits.multiCn;
		updEventSet(0x78);
	}
#if BOARD_SERIAL4
	updInput.bits.p77Last = updInput.bits.p77;
	updInput.bits.p77 = P7.7;
	if (updInput.bits.p77Last != updInput.bits.p77)
		updEventSet(4);
	updInput.bits.p73Last = updInput.bits.p73;
	updInput.bits.p73 = P7.3;
	if (updInput.bits.p73Last != updInput.bits.p73 && updInput.bits.p73)
		updEventSet(5);
#endif
	return 0;
}

/* event 0x78: what is on the multi connector (its level, 6 for nothing), then event 0x7a */
int multiCnCheck(void)
{
	if (input1Bit5Active())
		updMultiCn = analogReadMultiCn();
	else
		updMultiCn = 6;
	updEventSet(0x7a);
	return 0;
}

/* event 0x7a: the multi connector's output and link set for what is there */
int multiCnChanged(void)
{
	switch (updMultiCn) {
	case 2:
		powerMultiCnOff();
		boardSetMultiCnOutput(BOARD_MULTICN_HOST);
		serial1Reset();
		serialOpen1(2);
		break;
	case 1:
		serialClose1();
		serialOpen3(3);
		boardSetMultiCnOutput(BOARD_MULTICN_HOST);
		break;
	case 0xff:
	case 0:
	case 3:
	case 4:
	case 5:
	case 6:
	default:
		serialClose3();
		serialClose1();
		boardMultiCnStop();
		powerMultiCnOff();
		break;
	}
	return 0;
}
#else
/* the multi connector (P2.0), P7.7 and P7.3 followed: events 0x7a, 4 and 5 */
int inputPoll(void)
{
	updInput.bits.multiCnLast = updInput.bits.multiCn;
	updInput.bits.multiCn = !P2.0;
	if (updInput.bits.multiCn == updInput.bits.multiCnLast &&
	    updInput.bits.multiCn ^ updInput.bits.multiCnReported) {
		updInput.bits.multiCnReported = updInput.bits.multiCn;
		updEventSet(0x7a);
	}
	updInput.bits.p77Last = updInput.bits.p77;
	updInput.bits.p77 = P7.7;
	if (updInput.bits.p77Last != updInput.bits.p77)
		updEventSet(4);
	updInput.bits.p73Last = updInput.bits.p73;
	updInput.bits.p73 = P7.3;
	if (updInput.bits.p73Last != updInput.bits.p73 && updInput.bits.p73)
		updEventSet(5);
	return 0;
}

/* event 0x7a: the multi connector's output off, then on to what is there */
int multiCnChanged(void)
{
	boardSetMultiCnOutput(BOARD_MULTICN_HOST);
	powerMultiCnOff();
	if (boardMultiCnIsDongle()) {
		serialOpen1(2);
		sleepFactorSet6(1);
		sleepFactorSet5(1);
	} else {
		serial1Reset();
		serialClose1();
		sleepFactorSet6(0);
		sleepFactorSet5(0);
	}
	return 0;
}
#endif

/* event 4: P7.7 changed */
int p77Changed(void)
{
#if BOARD_SERIAL4
	if (intp11Low())
		serial4Open2(2);
	else
		serial4Close2();
#endif
	return 0;
}

#if FW_SINCE(0x0100060D)
#define UPD_MULTICN_CHECK()	multiCnCheck()
#else
#define UPD_MULTICN_CHECK()	updEventSet(0x79)
#endif

void mainLoop(void)
{
	unsigned char hostResult;
	unsigned char result;
	unsigned char count;

	serialReset(0);
	resetFlags = RESF;
	result = configStorageInit();
	if (result)
		UPD_LOG(("configStorageInit() =%d\r\n", result));
	result = updaterInit();
	if (result)
		UPD_LOG(("updaterInit() =%d\r\n", result));
	result = updaterInitErnieUpdMode();
	if (result)
		UPD_LOG(("updaterInitErnieUpdMode() =%d", result));
#if FW_SINCE(0x0100060D)
	if (updReset)
		;	/* Sony's tests updReset for a log line it does not keep */
#endif
	result = nvsInit();
	if (result)
		UPD_LOG(("nvsInit() error%d!\r\n", result));
#if FW_SINCE(0x0100060D)
	result = nvsRead(0x1c, 4, (unsigned char *)&idRecord1, 1);
#endif
	if (updReset) {
		result = nvsRead(0x11, 1, &count, 1);
		if (!result && count != 0xff) {
			count++;
			nvsWrite(0x11, 1, &count, 1);
		}
	}
	buttonInit();
	tau0PowerOn();
	tm00Start();
	timerInit();
	serial1Reset();
	serial4StartTimers();
#if !UPD_060D_FORMS
#if BOARD_SERIAL2
	serial2Init();
#endif
	updEventSet(4);
	serialStartTimer();
#endif
	powerInit();
	powerOn();
	EI();
	for (;;) {
		switch (updModeGet()) {
		case 0:
			updModeChange(1);
			if (updEventTake(0x78))
				UPD_MULTICN_CHECK();
			if (updEventTake(0x7a))
				multiCnChanged();
			break;
		case 1:
			if (updEventTake(0x67))
				updModeChange(3);
			if (updEventTake(0x15))
				updModeChange(3);
			if (updEventTake(0x66))
				updModeChange(5);
			if (updEventTake(0x69))
				updModeChange(5);
			if (updEventTake(0x32))
				serialEvent(4);
			if (updEventTake(0x33))
				serialEvent(2);
			if (updEventTake(0x34))
				serialEvent(8);
			if (updEventTake(0x35))
				serialEvent(0x10);
			if (updEventTake(1))
				serial4Event(0x400);
			if (updEventTake(2))
				serial4Event(0x200);
			if (updEventTake(3))
				serial4Event(0x800);
			if (updEventTake(4))
				p77Changed();
			if (updEventTake(5))
				updModeChange(5);
			if (updEventTake(0x78))
				UPD_MULTICN_CHECK();
			if (updEventTake(0x7a))
				multiCnChanged();
			if (updEventTake(0x82))
				chargeUpdate();
			break;
		case 2:
			if (updEventTake(0x66))
				updModeChange(5);
			if (updEventTake(0x69))
				updModeChange(5);
			if (updEventTake(0x82))
				chargeUpdate();
			break;
		case 3:
			if (updEventTake(0x66))
				updModeChange(5);
			if (updEventTake(0x69))
				updModeChange(5);
			if (updEventTake(0x14))
				updModeChange(1);
			if (updEventTake(0x16))
				updModeChange(1);
			if (updEventTake(0x1e))
				updModeChange(5);
			if (updEventTake(0x1f))
				updModeChange(5);
			if (updEventTake(0x20))
				updModeChange(6);
			if (updEventTake(0x32))
				serialEvent(4);
			if (updEventTake(0x33))
				serialEvent(2);
			if (updEventTake(0x34))
				serialEvent(8);
			if (updEventTake(0x35))
				serialEvent(0x10);
			if (updEventTake(1))
				serial4Event(0x400);
			if (updEventTake(2))
				serial4Event(0x200);
			if (updEventTake(3))
				serial4Event(0x800);
			if (updEventTake(4))
				p77Changed();
			if (updEventTake(5))
				updModeChange(5);
			if (updEventTake(0x78))
				UPD_MULTICN_CHECK();
			if (updEventTake(0x7a))
				multiCnChanged();
			if (updEventTake(0x82))
				chargeUpdate();
			if (P1.6)
				hostResult = hostPoll();
			if ((updLoops & 7) == 0)
				hostSlowRun();
			break;
		case 4:
			if (updEventTake(0x66))
				updModeChange(5);
			if (updEventTake(0x69))
				updModeChange(5);
			if (updEventTake(0x1e))
				updModeChange(5);
			if (updEventTake(0x1f))
				updModeChange(5);
			if (updEventTake(0x20))
				updModeChange(6);
			if (P1.6)
				hostResult = hostPoll();
			if (updEventTake(5))
				updModeChange(5);
			if ((updLoops & 7) == 0)
				hostSlowRun();
			break;
		}
#if UPD_060D_FORMS
		WDTE = 0xac;	/* the watchdog kept back */
#endif
		updLoops++;
	}
}

/* every 4 ms (TM00): P4.0 toggled, the timers' tick, the inputs, and a button press as its event */
void tm00Interrupt(void)
{
	P40 ^= 1;
	timerTick();
	inputPoll();
	switch (buttonPoll()) {
	case 1:
		updEventSet(0x64);
		break;
	case 3:
		updEventSet(0x66);
		break;
	case 4:
#if SERIAL2_ARM
		serial2Arm();
#endif
		updEventSet(0x67);
		break;
	case 5:
		updEventSet(0x68);
		break;
	case 6:
		updEventSet(0x69);
		break;
	}
}
