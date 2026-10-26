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
#include "downloader.h"
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
#include "updater.h"


unsigned char updMode;		/* updModeChange's */
unsigned char updReset;		/* started from the reset vector (main), not by the application */
unsigned char updVersion[4];	/* the running application's version */
char updBuild[12];		/* and its build time, digits only */

unsigned long updLoops;		/* the main loop's rounds */
union UpdEvents updEvents;
unsigned int buttonPowerHeld;	/* the polls P11.5 has been held low */
unsigned int buttonPsHeld;	/* and P4.6 */
unsigned long buttonPolls;
unsigned int buttonShortTime;	/* buttonPoll's times, in polls */
unsigned int buttonHoldTime1;
unsigned int buttonHoldTime2;
union UpdInput updInput;

void mainLoop(void);

/* sleepfactor.c's, reached as far data */
extern union SleepFactor sleepFactor;
extern union SleepFactor sleepFactorRead;
extern union SleepFactorStatus sleepFactorStatus;

void main(void)
{
	rtcInit();
	sleepFactorInit();
	hostLink.frameSize = 0x20;
	hostLink.config = 0;
	updReset = 1;
	memcpy(updVersion, APP_VERSION_INFO->version, 4);
	timeStampDigits(updBuild, APP_VERSION_INFO->timeStamp);
	boardMultiCnOutputOff();
	mainLoop();
}

/*
 * the downloader's start (DOWNLOADER_ENTRY), from the application's updaterStartDownloader:
 * the stack reset, the sleep factor and host link it hands over in DOWNLOADER_START taken,
 * every interrupt masked, and the main loop entered as after an update (updReset 0)
 */
void downloaderStart(void)
{
#asm
	MOVW	SP, #0D500H
#endasm
	sleepFactorStatus.value = DOWNLOADER_START->snapshot;
	sleepFactor.bytes[0] = DOWNLOADER_START->sleepFactor[0];
	sleepFactor.bytes[1] = DOWNLOADER_START->sleepFactor[1];
	sleepFactor.bytes[2] = DOWNLOADER_START->sleepFactor[2];
	sleepFactor.bytes[3] = DOWNLOADER_START->sleepFactor[3];
	hostLink.frameSize = DOWNLOADER_START->frameSize;
	hostLink.config = DOWNLOADER_START->link.config;
	updReset = 0;
	memcpy(updVersion, IMAGE_VERSION_INFO->version, 4);
	timeStampDigits(updBuild, (const char __far *)(IMAGE_VERSION_INFO + 1));
	MK0 = 0xffff;
	MK1 = 0xffff;
	MK2 = 0xffff;
	IF0 = 0;
	IF1 = 0;
	IF2 = 0;
	rtcInit();
	sleepFactorRead = sleepFactor;
	mainLoop();
}

int updaterInitErnieUpdMode(void)
{
	if (updReset) {
		updMode = 0;
		paramInit();
		watchdogSet(&configGet()->watchdog);
		sub_3B718();
		updInput.bits.multiCnReported = updInput.bits.multiCn = updInput.bits.multiCnLast =
			!P2.0 ^ 1;
	} else {
		updMode = 3;
		hostOpen();
		updaterDecryptInit();
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
			if (chargeMayStart()) {
				handshakeInit();
				watchdogSetup();
				updaterDecryptInit();
				powerResume();
				hostOpen();
				refused = 0;
			}
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
		case 2:
			powerOff();
			boardMultiCnOutputOff();
			sub_3B718();
			refused = 0;
			break;
		case 1:
			hostClose();
			powerOff();
			boardMultiCnOutputOff();
			sub_3B718();
			refused = 0;
			break;
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
		logPrintf("[UPDMOD] change %x to %x\r\n", updMode, mode);
		updMode = mode;
		updEventTake(0x66);
		updEventTake(0x69);
		updEventTake(0x64);
		updEventTake(0x67);
		updEventTake(0x14);
		updEventTake(0x15);
		serial4Answer0205();
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
	sub_3AC35();
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

/* event 4: P7.7 changed */
int p77Changed(void)
{
	if (sub_4D63())
		serial4Open2(2);
	else
		serial4Close2();
	return 0;
}

void mainLoop(void)
{
	unsigned char hostResult;
	unsigned char result;
	unsigned char count;

	serialReset(0);
	resetFlags = RESF;
	result = configStorageInit();
	if (result)
		logPrintf("configStorageInit() =%d\r\n", result);
	result = updaterInit();
	if (result)
		logPrintf("updaterInit() =%d\r\n", result);
	result = updaterInitErnieUpdMode();
	if (result)
		logPrintf("updaterInitErnieUpdMode() =%d", result);
	result = nvsInit();
	if (result)
		logPrintf("nvsInit() error%d!\r\n", result);
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
	sub_3AF74();
	serial1Reset();
	serial4StartTimers();
	serial2Init();
	updEventSet(4);
	serialStartTimer();
	powerInit();
	powerOn();
	EI();
	for (;;) {
		switch (updModeGet()) {
		case 0:
			updModeChange(1);
			if (updEventTake(0x78))
				updEventSet(0x79);
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
				updEventSet(0x79);
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
				updEventSet(0x79);
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
