/*
 * The power sequences' board parts (DEM-3000L): the devices read at the power-on, the Elmo
 * mask steps of the resume, off and suspend sequences with what goes around them, and the
 * devices' power.
 */
#pragma sfr

#include <string.h>

#include "ernie.h"
#include "battery.h"
#include "bbmc.h"
#include "board.h"
#include "charge.h"
#include "chip.h"
#include "clockout.h"
#include "config.h"
#include "cookie.h"
#include "elmo.h"
#include "iica.h"
#include "led.h"
#include "log.h"
#include "motion.h"
#include "param.h"
#include "powerjob.h"
#include "powerreq.h"
#include "powerseq.h"
#include "powerwait.h"
#include "serial2.h"
#include "sleepfactor.h"
#include "state.h"
#include "timer.h"
#if BOARD_DOL1001
#include "thermal.h"
#endif

#ifdef ERNIE_UPDATER
/* the updater's powerwait.c: the sequences' steps wait in its powerSleep, the devices in its
   powerSleep2 (powerSleep's name kept from the macro below by the parentheses) */
#define powerSleep(ms)		powerSleep2(ms)
#define powerWaitMs(ms)		(powerSleep)(ms)
extern unsigned char updReset;	/* updater.c's */
#endif

#if SOURCE_0103
/*
 * The slim's (USS-1001's) power sequences: the masks a step changes set or cleared and written
 * in one go (elmoUpdateMasks), the steps' devices around them, the WLAN's line on P1.7 and
 * the line P9.5 was on P0.6; no 3G modem, no accessory, no HDMI CEC power, no headphone
 * remote. Its order is the slim's.
 */
#include "eventlog.h"

extern unsigned char input2Bit0Get(void);

#ifndef ERNIE_UPDATER	/* the updater's are in its power.c */
struct DeviceVersions deviceVersions;
unsigned char elmoCommand[12];
#endif

#if BOARD_DOL1002
/* DOL-1002's: the RMR (register 0xa1 bit 13) with the resume's first step and the others' last;
   no 0x0200 in register 0xa2 */
static const struct PowerStep powerResumeSteps[4] = {
	{ { 0x0000, 0x4000, 0x0000, 0x0000, 0xa000, 0x0000 }, 6 },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 3 },
	{ { 0x7102, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 7 },
	{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0 },
};
static const struct PowerStep powerOffSteps[4] = {
	{ { 0x0011, 0x2000, 0x1000, 0x0000, 0x0000, 0x0000 }, 1 },
	{ { 0x7102, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x5b },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5b },
	{ { 0x0000, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x76 },
};
#ifndef ERNIE_UPDATER	/* the updater does not suspend */
static const struct PowerStep powerSuspendSteps[4] = {
	{ { 0x0001, 0x2000, 0x0000, 0x0000, 0x0000, 0x0000 }, 1 },
	{ { 0x7102, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x5b },
	{ { 0x8000, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5b },
	{ { 0x0000, 0x4000, 0x0000, 0x0000, 0xa000, 0x0000 }, 0x76 },
};
#endif
#elif BOARD_DOL1001
/* the PS TV's (DOL-1001's) */
static const struct PowerStep powerResumeSteps[4] = {
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 6 },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x0a },
	{ { 0x7102, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000 }, 8 },
	{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0 },
};
static const struct PowerStep powerOffSteps[4] = {
	{ { 0x0011, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000 }, 1 },
	{ { 0x7102, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x73 },
};
#ifndef ERNIE_UPDATER	/* the updater does not suspend */
static const struct PowerStep powerSuspendSteps[4] = {
	{ { 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 1 },
	{ { 0x7102, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0000, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 0x73 },
};
#endif
#else
static const struct PowerStep powerResumeSteps[4] = {
	{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xa000 }, 6 },
	{ { 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 2 },
	{ { 0x7980, 0x0000, 0x0040, 0x1400, 0x0000, 0x0000 }, 8 },
	{ { 0x0002, 0x0000, 0x0100, 0x0000, 0x0000, 0x0000 }, 7 },
};
static const struct PowerStep powerOffSteps[4] = {
	{ { 0x0013, 0x0000, 0x1180, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x7980, 0x0000, 0x0040, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x7c },
};
#ifndef ERNIE_UPDATER	/* the updater does not suspend */
static const struct PowerStep powerSuspendSteps[4] = {
	{ { 0x0003, 0x0000, 0x0180, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x7100, 0x0000, 0x0040, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xa000 }, 0x7c },
};
#endif
#endif

static unsigned char powerA1On(void);
static unsigned char powerA1Off(void);

#if BOARD_DOL1001
#if SOURCE_SLIM
/* DOL-1002's: the Elmo's version only, the Cookie's 0xfdfd and the battery's identity 0xfffc
   (it has neither); the temperature sensor set up, its identity not read */
void powerSeqOn(void)
{
	unsigned char i;
	unsigned char unused;
	union CookieStatus1 status;
	union CookieStatus2 status2;

	deviceVersions.cookie[1] = 0;
	deviceVersions.cookie[0] = 0;
	deviceVersions.elmo[1] = 0;
	deviceVersions.elmo[0] = 0;
	deviceVersions.battery.version[4] = 0;
	deviceVersions.battery.version[3] = 0;
	deviceVersions.battery.version[2] = 0;
	deviceVersions.battery.version[1] = 0;
	deviceVersions.battery.version[0] = 0;
	deviceVersions.battery.id = 0;
	elmoInit();
	cookieInit();
	batteryInit();
	thermalInit();
	if (iicaInit()) {
		deviceVersions.elmo[1] = 0xff;
		deviceVersions.elmo[0] = 0xff;
		deviceVersions.cookie[1] = 0xff;
		deviceVersions.cookie[0] = 0xff;
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
			deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xffff;
	} else {
		for (i = 0; i < 3; i++) {
			if (!elmoReadVersion(deviceVersions.elmo)) {
				elmoInitRegs();
				break;
			}
			if (i == 2) {
				deviceVersions.elmo[1] = 0xff;
				deviceVersions.elmo[0] = 0xff;
			}
		}
		deviceVersions.cookie[1] = 0xfd;
		deviceVersions.cookie[0] = 0xfd;
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
			deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xfffc;
	}
	if (deviceVersions.cookie[0] == 0x80) {
		cookieReadStatus1(status.byte);
		status2.word = 0;
		if (!status.bits.battery) {
			batteryFaults.byte[0] |= 0x01;
			if ((status.byte[0] & 0xe0) == 0xa0) {
				cookieUpdateBatteryCharge(0);
				batteryFaults.byte[0] |= 0x02;
			}
		}
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
			deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xfffc;
		if (!batteryReadBicTemp(&batteryFaults.word[2]) &&
		    !batteryReadTemp(&batteryFaults.word[3]))
			batteryFaults.byte[0] |= 0x08;
	} else if (deviceVersions.cookie[0] == 0) {
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
			deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xfffc;
	}
	cookieUpdateBatteryCharge(1);
}
#else
/* the versions, the temperature sensor's identity and the battery's (0xff and 0xffff where none
   answered); the battery's faults kept, not logged */
void powerSeqOn(void)
{
	unsigned char i;
	unsigned char unused;
	union CookieStatus1 status;
	union CookieStatus2 status2;

	deviceVersions.cookie[1] = 0;
	deviceVersions.cookie[0] = 0;
	deviceVersions.elmo[1] = 0;
	deviceVersions.elmo[0] = 0;
	deviceVersions.battery.version[4] = 0;
	deviceVersions.battery.version[3] = 0;
	deviceVersions.battery.version[2] = 0;
	deviceVersions.battery.version[1] = 0;
	deviceVersions.battery.version[0] = 0;
	deviceVersions.battery.id = 0;
	deviceVersions.thermal[1] = 0;
	deviceVersions.thermal[0] = 0;
	elmoInit();
	cookieInit();
	batteryInit();
	thermalInit();
	if (iicaInit()) {
		deviceVersions.elmo[1] = 0xff;
		deviceVersions.elmo[0] = 0xff;
		deviceVersions.cookie[1] = 0xff;
		deviceVersions.cookie[0] = 0xff;
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
			deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xffff;
		deviceVersions.thermal[1] = 0xff;
		deviceVersions.thermal[0] = 0xff;
	} else {
		for (i = 0; i < 3; i++) {
			if (!elmoReadVersion(deviceVersions.elmo)) {
				elmoInitRegs();
				break;
			}
			deviceVersions.elmo[1] = 0xff;
			deviceVersions.elmo[0] = 0xff;
		}
		for (i = 0; i < 3; i++) {
			if (!cookieReadVersion(deviceVersions.cookie)) {
				cookieInitRegs();
				break;
			}
			deviceVersions.cookie[1] = 0xff;
			deviceVersions.cookie[0] = 0xff;
			deviceVersions.battery.id = deviceVersions.battery.version[0] =
				deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
				deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xffff;
		}
		for (i = 0; i < 3; i++) {
			if (!(unsigned char)thermalReadManufacturer(&deviceVersions.thermal[0])) {
				thermalReadDevice(&deviceVersions.thermal[1]);
				thermalStart();
				break;
			}
			deviceVersions.thermal[1] = 0xff;
			deviceVersions.thermal[0] = 0xff;
		}
	}
	if (deviceVersions.cookie[0] == 0x80) {
#ifdef ERNIE_UPDATER	/* the updater's: no battery faults kept */
		WDTE = 0xac;
#else
		cookieReadStatus1(status.byte);
		status2.word = 0;
		if (!status.bits.battery) {
			batteryFaults.byte[0] |= 0x01;
			if ((status.byte[0] & 0xe0) == 0xa0) {
				cookieUpdateBatteryCharge(0);
				batteryFaults.byte[0] |= 0x02;
			}
		}
#endif
		for (i = 0; i < 6; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id)) {
				batteryInfoRead();
				cookieReadStatus1(status.byte);
				if (status.bits.battery) {
#ifndef ERNIE_UPDATER
					batteryFaults.byte[0] |= 0x04;
#endif
					break;
				}
			}
			if (configGet()->start.bits.batteryNoRetry)
				break;
			powerSleep(500);
			if (i == 5)
				logEvent(0x0d);
		}
#ifndef ERNIE_UPDATER
		if (!batteryReadBicTemp(&batteryFaults.word[2]) &&
		    !batteryReadTemp(&batteryFaults.word[3]))
			batteryFaults.byte[0] |= 0x08;
#endif
	} else if (deviceVersions.cookie[0] == 0) {
		for (i = 0; i < 3; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id)) {
				batteryInfoRead();
				break;
			}
		}
	}
#ifdef ERNIE_UPDATER
	WDTE = 0xac;
#endif
	cookieUpdateBatteryCharge(1);
	chargeInit();
}
#endif

/* the regulators' levels and P9.5, the power LED on (blinking at a wake-up 0x20), the resume's
   Elmo steps, then P8.0 and the USB's power; 0 or elmoWrite's error */
unsigned char powerSeqResume(void)
{
	unsigned char result;
	unsigned char i;
	union PowerRequest mode;

	mode = *stateGetWakeupFactor();
	powerSeqLevels();
	P95 = 1;
	if (mode.bits.mode == 0x20) {
		powerLedSetBlink(2, 500, 500);
		POWER_LED_START(POWER_LED_MODE_BLINK, POWER_LED_EVENT_RESUME_BLINK);
	} else
		POWER_LED_START(POWER_LED_MODE_ON, POWER_LED_EVENT_RESUME);
	powerWaitInit();
	for (i = 0; i < boardResumeSteps(); i++) {
		result = elmoUpdateMasks((struct ElmoMasks *)&powerResumeSteps[i].masks, 1);
		if (result != 0)
			goto out;
		powerWaitMs(powerResumeSteps[i].delay);
	}
	if (mode.bits.mode == 0x0c)
		powerSdOn();
	powerP80On();
	powerUsbOn();
#ifndef ERNIE_UPDATER
	if (elmoStatusMask.reg[0] & 0x10)
		powerSeqWaitP67();
#endif
out:
	TT0L = 0x08;
	TMMK03 = 1;
	return result;
}
#else
#if BATTERY_GAUGE
#ifdef ERNIE_UPDATER
extern unsigned char updResetGet(void);	/* updater.c's */

/* USS-1002's 0x0105 updater: the versions, the Elmo's and the Cookie's registers set up (the
   Cookie's register 3 bit 2 cleared after a reset only), the gauge powered on after a reset
   or its record and offsets loaded, the battery's information read once; the watchdog
   restarted before the charge goes back on */
void powerSeqOn(void)
{
	unsigned char i;
	unsigned char unused;		/* the application's locals, unused: Sony's frame keeps them */
	union CookieStatus1 status;
	union CookieStatus2 status2;

	deviceVersions.cookie[1] = 0;
	deviceVersions.cookie[0] = 0;
	deviceVersions.elmo[1] = 0;
	deviceVersions.elmo[0] = 0;
	deviceVersions.battery.version[4] = 0;
	deviceVersions.battery.version[3] = 0;
	deviceVersions.battery.version[2] = 0;
	deviceVersions.battery.version[1] = 0;
	deviceVersions.battery.version[0] = 0;
	deviceVersions.battery.id = 0;
	elmoInit();
	cookieInit();
	if (updResetGet())
		batteryInit();
	if (iicaInit()) {
		deviceVersions.elmo[1] = 0xff;
		deviceVersions.elmo[0] = 0xff;
		deviceVersions.cookie[1] = 0xff;
		deviceVersions.cookie[0] = 0xff;
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
			deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xffff;
	} else {
		for (i = 0; i < 3; i++) {
			if (!elmoReadVersion(deviceVersions.elmo)) {
				elmoInitRegs();
				break;
			}
			if (i == 2) {
				deviceVersions.elmo[1] = 0xff;
				deviceVersions.elmo[0] = 0xff;
			}
		}
		for (i = 0; i < 3; i++) {
			if (!cookieReadVersion(deviceVersions.cookie)) {
				cookieInitRegs();
				if (updResetGet())
					cookieCtrlReg3Bit2(0);
				break;
			}
			if (i == 2) {
				deviceVersions.cookie[1] = 0xff;
				deviceVersions.cookie[0] = 0xff;
				deviceVersions.battery.id = deviceVersions.battery.version[0] =
					deviceVersions.battery.version[1] =
					deviceVersions.battery.version[2] =
					deviceVersions.battery.version[3] =
					deviceVersions.battery.version[4] = 0xffff;
			}
		}
		if (updResetGet()) {
			batteryPowerOn();
		} else {
			batteryLoadRecord();
			batteryLoadOffsets();
		}
	}
	if (deviceVersions.cookie[0] == 0x80)
		batteryReadInfo(&deviceVersions.battery.id);
	else if (deviceVersions.cookie[0] == 0)
		batteryReadInfo(&deviceVersions.battery.id);
	WDTE = 0xac;
	cookieUpdateBatteryCharge(1);
}
#else
/* USS-1002's 0x0105: the versions (0xff and 0xffff where the third try failed), the gauge on;
   the battery's information read once, its faults logged (event log kind 5, codes 0x10 and
   0x11) */
void powerSeqOn(void)
{
	unsigned char i;
	unsigned char unused;
	union CookieStatus1 status;
	union CookieStatus2 status2;

	deviceVersions.cookie[1] = 0;
	deviceVersions.cookie[0] = 0;
	deviceVersions.elmo[1] = 0;
	deviceVersions.elmo[0] = 0;
	deviceVersions.battery.version[4] = 0;
	deviceVersions.battery.version[3] = 0;
	deviceVersions.battery.version[2] = 0;
	deviceVersions.battery.version[1] = 0;
	deviceVersions.battery.version[0] = 0;
	deviceVersions.battery.id = 0;
	elmoInit();
	cookieInit();
	batteryInit();
	if (iicaInit()) {
		deviceVersions.elmo[1] = 0xff;
		deviceVersions.elmo[0] = 0xff;
		deviceVersions.cookie[1] = 0xff;
		deviceVersions.cookie[0] = 0xff;
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
			deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xffff;
	} else {
		for (i = 0; i < 3; i++) {
			if (!elmoReadVersion(deviceVersions.elmo)) {
				elmoInitRegs();
				break;
			}
			if (i == 2) {
				deviceVersions.elmo[1] = 0xff;
				deviceVersions.elmo[0] = 0xff;
			}
		}
		for (i = 0; i < 3; i++) {
			if (!cookieReadVersion(deviceVersions.cookie)) {
				cookieInitRegs();
				cookieCtrlReg3Bit2(0);
				break;
			}
			if (i == 2) {
				deviceVersions.cookie[1] = 0xff;
				deviceVersions.cookie[0] = 0xff;
				deviceVersions.battery.id = deviceVersions.battery.version[0] =
					deviceVersions.battery.version[1] =
					deviceVersions.battery.version[2] =
					deviceVersions.battery.version[3] =
					deviceVersions.battery.version[4] = 0xffff;
			}
		}
		batteryPowerOn();
	}
	if (deviceVersions.cookie[0] == 0x80) {
		cookieReadStatus1(status.byte);
		cookieReadStatus2(status2.byte);
		if (!status.bits.battery) {
			batteryFaults.byte[0] |= 0x01;
			if ((status2.byte[0] & 0x0e) == 0x0a) {
				cookieUpdateBatteryCharge(0);
				batteryFaults.byte[0] |= 0x02;
			}
			eventLogRecord(5, 0x10, batteryFaults.byte, 4);
		}
		batteryReadInfo(&deviceVersions.battery.id);
		batteryFaults.byte[0] |= 0x04;
		eventLogRecord(5, 0x10, batteryFaults.byte, 4);
		if (!batteryReadBicTemp(&batteryFaults.word[2]) &&
		    !batteryReadTemp(&batteryFaults.word[3])) {
			batteryFaults.byte[0] |= 0x08;
			eventLogRecord(5, 0x11, (unsigned char *)&batteryFaults.word[2], 4);
			eventLogRecord(5, 0x10, batteryFaults.byte, 4);
		}
	} else if (deviceVersions.cookie[0] == 0) {
		batteryReadInfo(&deviceVersions.battery.id);
	}
	cookieUpdateBatteryCharge(1);
}
#endif
#else
/* the versions and the battery's identity (0xff and 0xffff where none answered); the battery's
   faults logged (event log kind 5, codes 0x10 and 0x11) */
void powerSeqOn(void)
{
	unsigned char i;
	unsigned char unused;
	union CookieStatus1 status;
	union CookieStatus2 status2;

	deviceVersions.cookie[1] = 0;
	deviceVersions.cookie[0] = 0;
	deviceVersions.elmo[1] = 0;
	deviceVersions.elmo[0] = 0;
	deviceVersions.battery.version[4] = 0;
	deviceVersions.battery.version[3] = 0;
	deviceVersions.battery.version[2] = 0;
	deviceVersions.battery.version[1] = 0;
	deviceVersions.battery.version[0] = 0;
	deviceVersions.battery.id = 0;
	elmoInit();
	cookieInit();
	batteryInit();
	if (iicaInit()) {
		deviceVersions.elmo[1] = 0xff;
		deviceVersions.elmo[0] = 0xff;
		deviceVersions.cookie[1] = 0xff;
		deviceVersions.cookie[0] = 0xff;
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
			deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xffff;
	} else {
		for (i = 0; i < 3; i++) {
			if (!elmoReadVersion(deviceVersions.elmo)) {
				elmoInitRegs();
				break;
			}
			deviceVersions.elmo[1] = 0xff;
			deviceVersions.elmo[0] = 0xff;
		}
		for (i = 0; i < 3; i++) {
			if (!cookieReadVersion(deviceVersions.cookie)) {
				cookieInitRegs();
#ifdef ERNIE_UPDATER	/* the slim updater's: after a reset only */
				if (updReset)
					cookieCtrlReg3Bit2(0);
#else
				cookieCtrlReg3Bit2(0);
#endif
				break;
			}
			deviceVersions.cookie[1] = 0xff;
			deviceVersions.cookie[0] = 0xff;
			deviceVersions.battery.id = deviceVersions.battery.version[0] =
				deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
				deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0xffff;
		}
	}
	if (deviceVersions.cookie[0] == 0x80) {
#ifdef ERNIE_UPDATER	/* the updater's: no battery faults kept */
		WDTE = 0xac;
#else
		cookieReadStatus1(status.byte);
		cookieReadStatus2(status2.byte);
		if (!status.bits.battery) {
			batteryFaults.byte[0] |= 0x01;
			if ((status2.byte[0] & 0x0e) == 0x0a) {
				cookieUpdateBatteryCharge(0);
				batteryFaults.byte[0] |= 0x02;
			}
#if BOARD_EVENT_LOG
			eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
		}
#endif
		for (i = 0; i < 6; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id)) {
				batteryInfoRead();
				cookieReadStatus1(status.byte);
				if (status.bits.battery) {
#ifndef ERNIE_UPDATER
					batteryFaults.byte[0] |= 0x04;
#if BOARD_EVENT_LOG
					eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
#endif
					break;
				}
			}
			if (configGet()->start.bits.batteryNoRetry)
				break;
			powerSleep(500);
			if (i == 5)
				logEvent(0x0d);
		}
#ifndef ERNIE_UPDATER
		if (!batteryReadBicTemp(&batteryFaults.word[2]) &&
		    !batteryReadTemp(&batteryFaults.word[3])) {
			batteryFaults.byte[0] |= 0x08;
#if BOARD_EVENT_LOG
			eventLogRecord(5, 0x11, (unsigned char *)&batteryFaults.word[2], 4);
			eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
		}
#endif
	} else if (deviceVersions.cookie[0] == 0) {
		for (i = 0; i < 3; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id)) {
				batteryInfoRead();
				break;
			}
		}
	}
#ifdef ERNIE_UPDATER
	WDTE = 0xac;
#endif
	cookieUpdateBatteryCharge(1);
	chargeInit();
}
#endif

/* the regulators' levels, P0.6 and the 0xa1 device on, then the resume's Elmo steps; 0 or
   elmoWrite's error */
unsigned char powerSeqResume(void)
{
	unsigned char result;
	unsigned char i;
	union PowerRequest mode;

	mode = *stateGetWakeupFactor();
	powerSeqLevels();
	P0.6 = 1;
	powerA1On();
	powerLedSetFlagsNow(0x01);
	powerWaitInit();
	for (i = 0; i < boardResumeSteps(); i++) {
		result = elmoUpdateMasks((struct ElmoMasks *)&powerResumeSteps[i].masks, 1);
		if (result != 0)
			goto out;
		powerWaitMs(powerResumeSteps[i].delay);
	}
	if (mode.bits.mode == 0x0c)
		powerSdOn();
#ifndef ERNIE_UPDATER	/* the slim updater's waits nothing */
	if (elmoStatusMask.reg[0] & 0x10)
		powerSeqWaitP67();
#endif
out:
	TT0L = 0x08;
	TMMK03 = 1;
	return result;
}
#endif

#if BOARD_DOL1001
/* the Elmo's register 0x0f set to 1, then its regulators 0x81-0x84 at the levels the settings
   give, trimmed */
void powerSeqLevels(void)
{
	unsigned char trim;
	unsigned char level;

#if BOARD_DOL1002
	/* DOL-1002: command 0x23, and the levels untrimmed */
	elmoCommand[0] = 0x23;
	elmoCommand[1] = 0x00;
	elmoCommand[2] = 0x00;
	elmoWrite(elmoCommand, 3);
	trim = 0;
#else
	elmoCommand[0] = 0x0f;
	elmoCommand[1] = 0x01;
	elmoCommand[2] = 0x00;
	elmoWrite(elmoCommand, 3);
	trim = configGet()->resumeTrim;
#endif
	level = configGet()->voltage81;
	elmoCommand[0] = 0x81;
	elmoCommand[1] = levelTrim(level, trim);
	elmoCommand[2] = 0x00;
	level = configGet()->voltage82;
	elmoCommand[3] = 0x82;
	elmoCommand[4] = levelTrim(level, trim);
	elmoCommand[5] = 0x00;
	level = configGet()->voltage83;
	elmoCommand[6] = 0x83;
	elmoCommand[7] = levelTrim(level, trim);
	elmoCommand[8] = 0x00;
#if !BOARD_DOL1002
	trim = configGet()->resumeTrim84;
#endif
	level = configGet()->voltage84;
	elmoCommand[9] = 0x84;
	elmoCommand[10] = levelTrim(level, trim);
	elmoCommand[11] = 0x00;
	elmoWrite(elmoCommand, 12);
}
#else
/* the Elmo's regulators 0x81-0x84 at the levels the settings give, trimmed */
void powerSeqLevels(void)
{
	unsigned char trim;
	unsigned char level;

	trim = configGet()->voltageTrim;
	level = configGet()->voltage81;
	elmoCommand[0] = 0x81;
	elmoCommand[1] = levelTrim(level, trim);
	elmoCommand[2] = 0x00;
	level = configGet()->voltage82;
	elmoCommand[3] = 0x82;
	elmoCommand[4] = levelTrim(level, trim);
	elmoCommand[5] = 0x00;
	level = configGet()->voltage83;
	elmoCommand[6] = 0x83;
	elmoCommand[7] = levelTrim(level, trim);
	elmoCommand[8] = 0x00;
#if BOARD_USS1001 && SOURCE_0104
	trim = configGet()->voltageTrim + 1;	/* USS-1002's 0x0105: 0x84 a step above (0x088e) */
#endif
	level = configGet()->voltage84;
	elmoCommand[9] = 0x84;
	elmoCommand[10] = levelTrim(level, trim);
	elmoCommand[11] = 0x00;
	elmoWrite(elmoCommand, 12);
}
#endif

#if BOARD_DOL1001
/* P3.1 low until P6.5 goes low, at most six waits (the updater's: nothing) */
void powerSeqWaitP67(void)
{
#ifndef ERNIE_UPDATER
	unsigned char i;

	P3.1 = 0;
	for (i = 0; i < 6; i++) {
		powerWaitMs(5);
		if (!P6.5)
			break;
	}
	P3.1 = 1;
#endif
}

/* the lines low, the devices off, then the off sequence's Elmo steps */
unsigned char powerSeqOff(void)
{
	unsigned char result;
	unsigned char i;

#ifndef ERNIE_UPDATER	/* the updater leaves the charge alone */
	if (stateGetCause() == 0x31)
		chargeSetManualMode(0);
#endif
	P13.1 = 0;
	P3.1 = 0;
	clockOutputStop();
	powerMultiCnOff();
	powerRmrOff();
	powerHdmiCecOff();
	powerUsbOff();
	powerP80Off();
	POWER_LED_START(POWER_LED_MODE_ON, POWER_LED_EVENT_OFF);
	powerWaitInit();
	for (i = 0; i < boardOffSteps(); i++) {
		result = elmoUpdateMasks((struct ElmoMasks *)&powerOffSteps[i].masks, 0);
		if (result != 0)
			goto out;
		powerWaitMs(powerOffSteps[i].delay);
	}
	P95 = 0;
	POWER_LED_START(POWER_LED_MODE_OFF, POWER_LED_EVENT_OFF_DONE);
	sleepFactorSet0(0);
	sleepFactorSet2(0);
	sleepFactorSet1(0);
	sleepFactorSet10(0);
out:
	TT0L = 0x08;
	TMMK03 = 1;
	return result;
}

#ifndef ERNIE_UPDATER	/* the updater does not suspend */
/* as the off sequence, with the lines and the USB's power kept */
unsigned char powerSeqSuspend(void)
{
	unsigned char result;
	unsigned char i;

	powerMultiCnOff();
	powerRmrOff();
	powerHdmiCecOff();
	POWER_LED_START(POWER_LED_MODE_ON, POWER_LED_EVENT_SUSPEND);
	powerWaitInit();
	for (i = 0; i < boardSuspendSteps(); i++) {
		result = elmoUpdateMasks((struct ElmoMasks *)&powerSuspendSteps[i].masks, 0);
		if (result != 0)
			goto out;
		powerWaitMs(powerSuspendSteps[i].delay);
	}
	POWER_LED_START(POWER_LED_MODE_OFF, POWER_LED_EVENT_SUSPEND_DONE);
	sleepFactorSet2(0);
	sleepFactorSet1(0);
	sleepFactorSet10(0);
out:
	TT0L = 0x08;
	TMMK03 = 1;
	return result;
}
#endif
#else
/* the WLAN's line (P1.7) low until P12.0 goes low, at most six waits */
void powerSeqWaitP67(void)
{
#ifndef ERNIE_UPDATER	/* the slim updater's: nothing */
	unsigned char i;

	P1.7 = 0;
	for (i = 0; i < 6; i++) {
		powerWaitMs(5);
		if (!P12.0)
			break;
	}
	P1.7 = 1;
#endif
}

/* the lines low, the devices off, then the off sequence's Elmo steps */
unsigned char powerSeqOff(void)
{
	unsigned char result;
	unsigned char i;

#ifndef ERNIE_UPDATER
	if (stateGetCause() == 0x31)
		chargeSetManualMode(0);
#endif
	P4.1 = 0;
	P4.2 = 0;
#if !(BOARD_USS1001 && SOURCE_0104)	/* USS-1002's 0x0105 has no P6.5 line */
	P6.5 = 0;
#endif
	P0.5 = 0;
	P1.7 = 0;
	clockOutputStop();
	powerMultiCnOff();
	powerRmrOff();
	powerLedClearFlagsNow(0x08);
	powerWaitInit();
	for (i = 0; i < boardOffSteps(); i++) {
		result = elmoUpdateMasks((struct ElmoMasks *)&powerOffSteps[i].masks, 0);
		if (result != 0)
			goto out;
		powerWaitMs(powerOffSteps[i].delay);
	}
	P0.6 = 0;
	powerLedClearFlagsNow(0x01);
	powerA1Off();
	sleepFactorSet0(0);
	sleepFactorSet2(0);
	sleepFactorSet1(0);
	sleepFactorSet10(0);
out:
	TT0L = 0x08;
	TMMK03 = 1;
	return result;
}

#ifndef ERNIE_UPDATER	/* the updater does not suspend */
/* as the off sequence, with the clock output and P0.6 kept while the Elmo's mask 0x10 is set */
unsigned char powerSeqSuspend(void)
{
	unsigned char result;
	unsigned char i;

	P4.1 = 0;
	P4.2 = 0;
	if (!(elmoStatusMask.reg[0] & 0x10))
		clockOutputStop();
	else
		P1.7 = 1;
	powerMultiCnOff();
	powerRmrOff();
	powerLedClearFlagsNow(0x08);
	powerWaitInit();
	for (i = 0; i < boardSuspendSteps(); i++) {
		result = elmoUpdateMasks((struct ElmoMasks *)&powerSuspendSteps[i].masks, 0);
		if (result != 0)
			goto out;
		powerWaitMs(powerSuspendSteps[i].delay);
	}
	if (!(elmoStatusMask.reg[0] & 0x10))
		P0.6 = 0;
	powerLedClearFlagsNow(0x01);
	powerA1Off();
	sleepFactorSet2(0);
	sleepFactorSet1(0);
	sleepFactorSet10(0);
out:
	TT0L = 0x08;
	TMMK03 = 1;
	return result;
}
#endif
#endif

/* no accessory */
unsigned char powerAccOn(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerAccOff(void)
{
	unsigned char result;

	result = 0;
	return result;
}

/* the WLAN's lines: the PS TV's on P3.1, P13.1 and P9.5 */
#if BOARD_DOL1001
#define POWER_WLAN_PIN		P3.1
#define POWER_OFF_PIN		P13.1
#define POWER_RESUME_PIN	P95
#else
#define POWER_WLAN_PIN		P1.7
#define POWER_OFF_PIN		P0.5
#define POWER_RESUME_PIN	P0.6
#endif

unsigned char powerOnWlan(void)
{
	unsigned char result;
	unsigned int mask0;
	unsigned int mask2;

	if (elmoStatusMask.reg[0] & 0x0010) {
		result = 0;
	} else {
		powerWaitTimers(0x20);
		clockOutputStart();
		powerSleep(1);
		mask0 = elmoStatusMask.reg[0] | 0x0010;
		mask2 = elmoStatusMask.reg[2] | 0x1000;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask0)[1];
		elmoCommand[2] = (unsigned char)mask0;
		elmoCommand[3] = 0xa2;
		elmoCommand[4] = ((unsigned char *)&mask2)[1];
		elmoCommand[5] = (unsigned char)mask2;
		result = elmoWrite(elmoCommand, 6);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask0;
			elmoStatusMask.reg[2] = mask2;
			sleepFactorSet0(1);
			powerSleep(2);
			POWER_WLAN_PIN = 1;
		}
	}
	return result;
}

unsigned char powerOffWlan(void)
{
	unsigned char result;
	unsigned int mask0;
	unsigned int mask2;

#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
	if (elmoStatusMask.reg[0] & 0x0010 || elmoStatusMask.reg[2] & 0x1000) {
#else
	if (elmoStatusMask.reg[0] & 0x0010) {
#endif
		POWER_OFF_PIN = 0;
		clockOutputStop();
		POWER_WLAN_PIN = 0;
		mask0 = elmoStatusMask.reg[0] & 0xffef;
		mask2 = elmoStatusMask.reg[2] & 0xefff;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask0)[1];
		elmoCommand[2] = (unsigned char)mask0;
		elmoCommand[3] = 0xa2;
		elmoCommand[4] = ((unsigned char *)&mask2)[1];
		elmoCommand[5] = (unsigned char)mask2;
		result = elmoWrite(elmoCommand, 6);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask0;
			elmoStatusMask.reg[2] = mask2;
#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
		}
		if (result == 0) {
#endif
			sleepFactorSet0(0);
			powerStartTimer(0x20, POWER_TIMER(9), 0x64);
			if (stateGet() & 0x080c)
				POWER_RESUME_PIN = 0;
		}
	} else {
		result = 0;
	}
	return result;
}

/* a regulator's level (0x81-0x84); the result is not set for another register */
unsigned char powerSetVoltage(unsigned char regulator, unsigned char level)
{
	unsigned char result;

	if (regulator >= 0x81 && regulator < 0x85) {
		elmoCommand[0] = regulator;
		elmoCommand[1] = level;
		elmoCommand[2] = 0;
		result = elmoWrite(elmoCommand, 3);
		powerSleep(2);
	}
	return result;
}

/* the Cookie's register 6's bit 3 cleared, and set */
unsigned char powerCookieSet(void)
{
#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
	unsigned char result;

	result = cookieUpdateReg6Bit3(0);
	return result;
#else
	return cookieUpdateReg6Bit3(0);
#endif
}

unsigned char powerCookieClear(void)
{
	return cookieUpdateReg6Bit3(1);
}

/* the RMR's power: the Elmo's register 0xa1 bit 13 (DOL-1001's: register 0xaa bit 15) */
#if BOARD_DOL1001 && !BOARD_DOL1002
#define POWER_RMR_ELMO_REG	0xaa
#define POWER_RMR_REG		elmoRegAA
#define POWER_RMR_BIT		0x8000
#else
#define POWER_RMR_ELMO_REG	0xa1
#define POWER_RMR_REG		elmoStatusMask.reg[1]
#define POWER_RMR_BIT		0x2000
#endif
unsigned char powerRmrOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (POWER_RMR_REG & POWER_RMR_BIT) {
		result = 0;
	} else {
		powerWaitTimers(0x1000);
		mask = POWER_RMR_REG | POWER_RMR_BIT;
		elmoCommand[0] = POWER_RMR_ELMO_REG;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			POWER_RMR_REG = mask;
			powerSleep(0x0d);
		}
	}
	return result;
}

unsigned char powerRmrOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (POWER_RMR_REG & POWER_RMR_BIT) {
		mask = POWER_RMR_REG & ~POWER_RMR_BIT;
		elmoCommand[0] = POWER_RMR_ELMO_REG;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			POWER_RMR_REG = mask;
			powerStartTimer(0x1000, TIMER(0x0d), 0x4b);
		}
	} else {
		result = 0;
	}
	return result;
}

unsigned char powerSdOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[0] & 0x0001) {
		result = 0;
	} else {
		powerWaitTimers(0x2000);
		mask = elmoStatusMask.reg[0] | 0x0001;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask;
#if BOARD_DOL1001
			timerStart(TIMER(0x12), 1);
#else
			timerStart(TIMER(0x12), 0);
#endif
		}
	}
	return result;
}

unsigned char powerSdOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[0] & 0x0001) {
		mask = elmoStatusMask.reg[0] & 0xfffe;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask;
			timerStop(TIMER(0x12));
			sleepFactorSet10(0);
			powerStartTimer(0x2000, TIMER(0x0e), 0x11f);
		}
	} else {
		result = 0;
	}
	return result;
}

unsigned char powerMultiCnOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoRegAB & 0x8000) {
		result = 0;
	} else {
		mask = elmoRegAB | 0x8000;
		elmoCommand[0] = 0xab;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoRegAB = mask;
			powerSleep(0x28);
		}
	}
	return result;
}

unsigned char powerMultiCnOff(void)
{
	unsigned char result;
	unsigned int mask;

	mask = elmoRegAB & 0x7fff;
	elmoCommand[0] = 0xab;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoRegAB = mask;
		powerSleep(0x64);
	}
	return result;
}

#if BATTERY_GAUGE
unsigned char powerRegAA15On(void)
{
	unsigned char result;
	unsigned int mask;

	mask = elmoRegAA | 0x8000;
	elmoCommand[0] = 0xaa;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0)
		elmoRegAA = mask;
	return result;
}

unsigned char powerRegAA15Off(void)
{
	unsigned char result;
	unsigned int mask;

	mask = elmoRegAA & 0x7fff;
	elmoCommand[0] = 0xaa;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0)
		elmoRegAA = mask;
	return result;
}
#endif

#if BOARD_DOL1001
/* none on the PS TV */
static unsigned char powerA1On(void)
{
	unsigned char result;

	result = 0;
	return result;
}

static unsigned char powerA1Off(void)
{
	unsigned char result;

	result = 0;
	return result;
}
#else
/* the Elmo's register 0xa1 bit 14, on through the resume and off at the off and the suspend */
static unsigned char powerA1On(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[1] & 0x4000) {
		result = 0;
	} else {
		mask = elmoStatusMask.reg[1] | 0x4000;
		elmoCommand[0] = 0xa1;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[1] = mask;
			powerSleep(6);
		}
	}
	return result;
}

static unsigned char powerA1Off(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[1] & 0x4000) {
		mask = elmoStatusMask.reg[1] & 0xbfff;
		elmoCommand[0] = 0xa1;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0)
			elmoStatusMask.reg[1] = mask;
	} else {
		result = 0;
	}
	return result;
}
#endif

#if BOARD_USS1001 && SOURCE_0104
/* USS-1002's 0x0105: the touch panels' power, the Elmo's register 0xa0 bit 4 and 0xa2 bit 6
   (request 8's 0x08d0) */
unsigned char powerOnTouch(void)
{
	unsigned char result;
	unsigned int mask0;
	unsigned int mask2;

	if (elmoStatusMask.reg[0] & 0x0010 && elmoStatusMask.reg[2] & 0x0040) {
		result = 0;
	} else {
		mask0 = elmoStatusMask.reg[0] | 0x0010;
		mask2 = elmoStatusMask.reg[2] | 0x0040;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask0)[1];
		elmoCommand[2] = (unsigned char)mask0;
		elmoCommand[3] = 0xa2;
		elmoCommand[4] = ((unsigned char *)&mask2)[1];
		elmoCommand[5] = (unsigned char)mask2;
		result = elmoWrite(elmoCommand, 6);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask0;
			elmoStatusMask.reg[2] = mask2;
			powerSleep(3);
		}
	}
	return result;
}

unsigned char powerOffTouch(void)
{
	unsigned char result;
	unsigned int mask0;
	unsigned int mask2;

	if (elmoStatusMask.reg[0] & 0x0010 || elmoStatusMask.reg[2] & 0x0040) {
		mask0 = elmoStatusMask.reg[0] & 0xffef;
		mask2 = elmoStatusMask.reg[2] & 0xffbf;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask0)[1];
		elmoCommand[2] = (unsigned char)mask0;
		elmoCommand[3] = 0xa2;
		elmoCommand[4] = ((unsigned char *)&mask2)[1];
		elmoCommand[5] = (unsigned char)mask2;
		result = elmoWrite(elmoCommand, 6);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask0;
			elmoStatusMask.reg[2] = mask2;
			powerSleep(300);
		}
	} else {
		result = 0;
	}
	return result;
}
#endif

/* timer 0x11 (powerAccOn) */
void powerAccTimer(void)
{
	sleepFactorSet2(1);
}

/* timer 0x12 (powerSdOn) */
void powerSdTimer(void)
{
	sleepFactorSet10(1);
}

void powerSeqInit(void)
{
	struct TimerHandler handler;

	handler.handler = powerAccTimer;
	timerSetHandler(TIMER(0x11), &handler);
	handler.handler = powerSdTimer;
	timerSetHandler(TIMER(0x12), &handler);
}

#if BOARD_DOL1001
/* the PS TV's, PCH-1000's: at the off and the suspend, the charge cut for a while (timer 0x1e)
   while the Cookie's status 1 reads 4 and its register 3's bit 7 is clear */
void powerCookieHold(void)
{
#ifndef ERNIE_UPDATER	/* the updater's: nothing */
	cookieReadReg3();
	if ((cookieStatus1.byte[0] & 0xe0) == 0x80 && !(cookieRegs[0].byte[0] & 0x80)) {
		cookieCtrlCharge(0);
		chargeCookieHeld = 1;
		timerStart(TIMER_RTC(0x1e), 2);
		return;
	}
	chargeCookieHeld = 0;
#endif
}

/* task 5's, after timer 0x1e */
void powerCookieRelease(void)
{
#ifndef ERNIE_UPDATER
	chargeCookieHeld = 0;
	cookieCtrlCharge(1);
#endif
}
#else
/* at the off and the suspend: the Cookie's register 3 bit 13 set while its status is 4 and
   register 6's bit 3 is clear, the power LED's cause 0x20 shown */
void powerCookieHold(void)
{
#ifndef ERNIE_UPDATER	/* the updater's: nothing */
	cookieReadReg3();
	if ((cookieStatus2.byte[0] & 0x0e) == 0x08 && !(cookieRegs[3].byte[0] & 0x08)) {
		powerLedSetFlags(0x20);
		cookieCtrlReg3Bit13(1);
	}
#endif
}

/* task 5's, after the hold */
void powerCookieRelease(void)
{
#ifndef ERNIE_UPDATER
	chargeCookieHeld = 0;
	powerLedSetFlags(0x20);
	cookieCtrlCharge(1);
#endif
}
#endif

/* no 3G modem */
unsigned char powerBbmcCycle(void)
{
	powerBbmcOff();
	return powerBbmcOn();
}

unsigned char powerBbmcOff(void)
{
	unsigned char result;

	result = 0;
	result = 4;
	return result;
}

unsigned char powerBbmcOn(void)
{
	unsigned char result;

	result = 4;
#if !(BOARD_USS1001 && SOURCE_0104)	/* USS-1002's 0x0105 wakes no BBMC */
	if (result)
		bbmcWake();
#endif
	return result;
}

void powerBbmcPulse(void)
{
}

/* power device job 0x2000: sleep factor 9 cleared */
unsigned char powerBbmcStop(void)
{
	sleepFactorSet9(0);
	return 0;
}

#if BOARD_DOL1001
/* the USB's power (P3.6, serial command 0x08c5): on once the timer of its off (0x4000) ran out */
unsigned char powerUsbOn(void)
{
	unsigned char result;

	result = 0;
	if (P36) {
		/* on already */
	} else {
		powerWaitTimers(0x4000);
		P36 = 1;
		powerSleep(10);
	}
	return result;
}

unsigned char powerUsbOff(void)
{
	unsigned char result;

	result = 0;
	if (P36) {
		P36 = 0;
		timerStop(0x13);
		powerStartTimer(0x4000, 0x13, 0x25);
	}
	return result;
}

/* the HDMI CEC's power (P37) */
unsigned char powerHdmiCecOn(void)
{
	unsigned char result;

	result = 0;
	if (P37) {
		/* on already */
	} else {
		powerWaitTimers(0x0002);
		P37 = 1;
		powerSleep(20);
	}
	return result;
}

unsigned char powerHdmiCecOff(void)
{
	unsigned char result;

	result = 0;
	if (P37) {
		P37 = 0;
		powerStartTimer(0x0002, 4, 0x32);
	}
	return result;
}

/* the power LED (P14.5) */
unsigned char powerLedSet(unsigned char on)
{
	if (on == 1)
		P14.5 = 1;
	else
		P14.5 = 0;
	return 0;
}

/* P8.0, on through the resume and off at the off */
unsigned char powerP80On(void)
{
	unsigned char result;

	result = 0;
	if (P8.0) {
		/* on already */
	} else {
		powerWaitTimers(0x8000);
		P8.0 = 1;
		powerSleep(20);
	}
	return result;
}

unsigned char powerP80Off(void)
{
	unsigned char result;

	result = 0;
	if (P8.0) {
		P8.0 = 0;
#if BOARD_DOL1002
		powerStartTimer(0x8000, 0x16, 0xfa);
#else
		powerStartTimer(0x8000, 0x15, 0xfa);
#endif
	}
	return result;
}

/* task 5's (flag 0x0200): sleep factor 21 while P7.4 is low */
void powerSleepFactor21(void)
{
	if (P7.4) {
		sleepFactorSet21(0);
		return;
	}
	sleepFactorSet21(1);
	return;
}

/* no LED brightness on the PS TV */
int powerLedSetBrightness(unsigned char level)
{
	return 0;
}

int powerLedSetPwm(unsigned char on)
{
	return 0;
}
#else
/* no HDMI CEC power */
unsigned char powerHdmiCecOn(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerHdmiCecOff(void)
{
	unsigned char result;

	result = 0;
	return result;
}

/* the Elmo's LED register's bit 15 */
unsigned char powerLedSet(unsigned char on)
{
	unsigned char result;
	unsigned int led;

	if (on == 1)
		led = elmoLed | 0x8000;
	else
		led = elmoLed & 0x7fff;
	elmoCommand[0] = 0xa5;
	elmoCommand[1] = ((unsigned char *)&led)[1];
	elmoCommand[2] = (unsigned char)led;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0)
		elmoLed = led;
	return result;
}

/* 0-15, into the Elmo's register 0x60 bits 12-15 (led.c's elsewhere) */
int powerLedSetBrightness(unsigned char level)
{
	unsigned char result;
	unsigned int value;

	if (level >= 16)
		level = 15;
	value = (elmoReg60 & 0x0fff) | ((unsigned int)level << 12);
	elmoCommand[0] = 0x60;
	elmoCommand[1] = ((unsigned char *)&value)[1];
	elmoCommand[2] = ((unsigned char *)&value)[0];
	result = elmoWrite(elmoCommand, 3);
	if (result == 0)
		elmoReg60 = value;
	return result;
}

/* the Elmo's register 0xa6 bit 15: the brightness used */
int powerLedSetPwm(unsigned char on)
{
	unsigned char result;
	unsigned int value;

	if (on == 1)
		value = elmoRegA6 | 0x8000;
	else
		value = elmoRegA6 & 0x7fff;
	elmoCommand[0] = 0xa6;
	elmoCommand[1] = ((unsigned char *)&value)[1];
	elmoCommand[2] = ((unsigned char *)&value)[0];
	result = elmoWrite(elmoCommand, 3);
	if (result == 0)
		elmoRegA6 = value;
	return result;
}
#endif

#ifndef ERNIE_UPDATER	/* the updater has none of the requests' controls */
/* 0x0884: Elmo register 0xa0's bit 14 or bit 13 set (the second wins when both are asked) */
unsigned char powerElmoA0Set(unsigned char bit14, unsigned char bit13)
{
	unsigned char result;
	unsigned int mask;

#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
	mask = elmoStatusMask.reg[0];
#endif
	powerWaitTimers(0x40);
	if (bit14)
		mask = elmoStatusMask.reg[0] | 0x4000;
	if (bit13)
		mask = elmoStatusMask.reg[0] | 0x2000;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerSleep(2);
	}
	return result;
}

/* and clear */
unsigned char powerElmoA0Clear(unsigned char bit14, unsigned char bit13)
{
	unsigned char result;
	unsigned int mask;

#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
	mask = elmoStatusMask.reg[0];
#elif BOARD_DOL1001
	powerWaitTimers(0x40);
#endif
	if (bit14)
		mask = elmoStatusMask.reg[0] & 0xbfff;
	if (bit13)
		mask = elmoStatusMask.reg[0] & 0xdfff;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerStartTimer(0x40, POWER_TIMER(0x0a), 0x1e);
	}
	return result;
}

/* 0x0885: Elmo register 0xa0's bit 12 set */
unsigned char powerElmoA0Set2(void)
{
	unsigned char result;
	unsigned int mask;

	powerWaitTimers(0x80);
	mask = elmoStatusMask.reg[0] | 0x1000;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerSleep(2);
	}
	return result;
}

/* and clear */
unsigned char powerElmoA0Clear2(void)
{
	unsigned char result;
	unsigned int mask;

	mask = elmoStatusMask.reg[0] & 0xefff;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerStartTimer(0x80, POWER_TIMER(0x0b), 0x1e);
	}
	return result;
}

#if BOARD_DOL1001
/* no motion sensor on the PS TV */
unsigned char powerMotionOn(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerMotionOff(void)
{
	unsigned char result;

	result = 0;
	return result;
}
#else
unsigned char powerMotionOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[2] & 0x0080) {
		result = 0;
	} else {
		powerWaitTimers(4);
		mask = elmoStatusMask.reg[2] | 0x0080;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
			powerSleep(3);
		}
	}
	return result;
}

/* the mask is cleared before the write */
unsigned char powerMotionOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[2] & 0x0080) {
#if !(BOARD_USS1001 && SOURCE_0104)	/* USS-1002's 0x0105 has no P6.5 line */
		P6.5 = 0;
#endif
		mask = elmoStatusMask.reg[2] &= 0xff7f;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
			powerStartTimer(4, POWER_TIMER(6), 0x4b);
		}
	} else {
		result = 0;
	}
	return result;
}
#endif

/* no headphone remote, no 3G modem to start */
unsigned char powerHpremoteOn(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerHpremoteOff(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerBbmcStart(void)
{
	unsigned char result;

	result = 4;
	return result;
}
#endif	/* ERNIE_UPDATER */

#if !UPD_0103
/* serial command 0x01b0: the Elmo's registers 0xb0-0xb3 as the last power cut read them */
void powerGetCutRegs(unsigned char *regs)
{
	memcpy(regs, powerCutRegs, 8);
}
#endif

#if (BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104) && !(defined(ERNIE_UPDATER))
/* (the updater's 0x088f is its own, command.c) */
/* 0x088f's work (powerRequestCtrlDeviceReset's before 0x0104): devices' lines held in reset
   (hold 1) or let go; DOL-1001's: the dummy byte, then P13.1 */
void powerCtrlDeviceReset(unsigned char hold, unsigned char devices)
{
	if (hold < 2) {
		if (devices & 0x01)
			;
#if BOARD_USS1001
		/* USS-1002's 0x0105: the slim's lines but P6.5 */
		if (devices & 0x02)
			;
		if (devices & 0x04)
			P4.2 = hold;
		if (devices & 0x08)
			P4.1 = hold;
		if (devices & 0x10)
			P0.5 = hold;
#else
		if (devices & 0x02)
			boardDummyPin = hold;
		if (devices & 0x04)
			boardDummyPin = hold;
		if (devices & 0x08)
			boardDummyPin = hold;
		if (devices & 0x10)
			P13.1 = hold;
#endif
	}
}
#endif
#else
#if HW_IRT001 && FW_SINCE(0x01000104)
/* 0x00315200 drives no WLAN line (before 0x01000104 the config's, as below) */
#define POWER_SET_WLAN_LINE(level)	do { } while (0)
#elif BOARD_WLAN_102603
#define POWER_SET_WLAN_LINE(level)	(P6.7 = (level))
#elif !FW_SINCE(0x00090804)
/* 0x00090206 drives no WLAN line */
#define POWER_SET_WLAN_LINE(level)	do { } while (0)
#elif FW_SINCE(0x01000104)
#define POWER_SET_WLAN_LINE(level)	(P31 = (level))
#else
/* before 0x01000104 P3.1 is driven only when the config says the WLAN's line is there */
#define POWER_SET_WLAN_LINE(level)	do { if (configGet()->CONFIG_WLAN_LINE) P31 = (level); } while (0)
#endif

extern unsigned char input2Bit0Get(void);
extern unsigned char input2Bit1Get(void);

#ifndef ERNIE_UPDATER	/* the updater's are in its power.c */
struct DeviceVersions deviceVersions;
unsigned char elmoCommand[12];
#endif

/* the HDMI output's status bits (BOARD_HDMI) */
#if BOARD_HDMI
#define POWER_A1_HDMI		0x8000
#define POWER_A2_HDMI		0x0200
#else
#define POWER_A1_HDMI		0x0000
#define POWER_A2_HDMI		0x0000
#endif

#if BOARD_IRS1001
/* IRS-1001's: four steps each */
static const struct PowerStep powerResumeSteps[4] = {
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 6 },
	{ { 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 4 },
	{ { 0x7980, 0x0000, 0x0040, 0x1400, 0x0000, 0x0000 }, 0x0a },
	{ { 0x0002, 0x0000, 0x0100, 0x0000, 0x0000, 0x0000 }, 8 },
};
static const struct PowerStep powerOffSteps[4] = {
	{ { 0x0013, 0x0000, 0x1180, 0x0200, 0x8000, 0x0000 }, 0x5c },
	{ { 0x7980, 0x0000, 0x0040, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x7c },
};
static const struct PowerStep powerSuspendSteps[4] = {
	{ { 0x0003, 0x0000, 0x0180, 0x0200, 0x8000, 0x0000 }, 0x5c },
	{ { 0x7100, 0x0000, 0x0040, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 0x7c },
};
#elif HW_IRT001
/* 0x00315200's: four steps each, 0x00102603's without register 0xa3's bits 15 and 14 and
   register 0xa2's 0x1880 */
static const struct PowerStep powerResumeSteps[4] = {
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 6 },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 4 },
	{ { 0x7103, 0x0000, 0x0140, 0x0000, 0x0000, 0x0000 }, 4 },
	{ { 0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000 }, 8 },
};
static const struct PowerStep powerOffSteps[4] = {
	{ { 0x0011, 0x8000, 0x1a80, 0x0000, 0x8000, 0x0000 }, 0x78 },
	{ { 0x7102, 0x0000, 0x0140, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x70 },
};
static const struct PowerStep powerSuspendSteps[4] = {
	{ { 0x0001, 0x8000, 0x0280, 0x0000, 0x8000, 0x0000 }, 0x78 },
	{ { 0x7102, 0x0000, 0x0140, 0x0000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 0x70 },
};
#elif BOARD_102603
/* 0x00102603's: four steps each */
static const struct PowerStep powerResumeSteps[4] = {
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 6 },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 4 },
	{ { 0x7103, 0x0000, 0x0140, 0xc000, 0x0000, 0x0000 }, 4 },
	{ { 0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000 }, 8 },
};
static const struct PowerStep powerOffSteps[4] = {
	{ { 0x0011, 0x8000, 0x1a80, 0x0000, 0x8000, 0x0000 }, 0x78 },
	{ { 0x7102, 0x0000, 0x19c0, 0xc000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x74 },
};
static const struct PowerStep powerSuspendSteps[4] = {
	{ { 0x0001, 0x8000, 0x0280, 0x0000, 0x8000, 0x0000 }, 0x78 },
	{ { 0x7102, 0x0000, 0x0140, 0xc000, 0x0000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 0x74 },
};
#else
static const struct PowerStep powerResumeSteps[3] = {
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 6 },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 4 },
	{ { 0x7102, 0x0000, 0x0140 | POWER_A2_HDMI, 0x0000, 0x0000, 0x0000 }, 8 },
};
static const struct PowerStep powerOffSteps[3] = {
	{ { 0x7113, POWER_A1_HDMI, 0x19c0 | POWER_A2_HDMI, 0x0000, 0x8000, 0x0000 }, 0x5c },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x70 },
};
static const struct PowerStep powerSuspendSteps[3] = {
	{ { 0x7103, POWER_A1_HDMI, 0x09c0 | POWER_A2_HDMI, 0x0000, 0x8000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 0x70 },
};
#endif

/* the versions and the battery's identity (0xff and 0xffff where none answered) */
void powerSeqOn(void)
{
	unsigned char i;
#if FW_SINCE(0x00090006)
	unsigned char unused;
	unsigned char status;
#endif

	deviceVersions.cookie[1] = 0;
	deviceVersions.cookie[0] = 0;
	deviceVersions.elmo[1] = 0;
	deviceVersions.elmo[0] = 0;
	deviceVersions.battery.version[2] = 0;
	deviceVersions.battery.version[1] = 0;
	deviceVersions.battery.version[0] = 0;
	deviceVersions.battery.id = 0;
	elmoInitRegs();
	cookieInit();
	batteryInit();
	if (iicaInit()) {
		deviceVersions.cookie[1] = 0xff;
		deviceVersions.cookie[0] = 0xff;
		deviceVersions.elmo[1] = 0xff;
		deviceVersions.elmo[0] = 0xff;
		deviceVersions.battery.id = deviceVersions.battery.version[0] =
			deviceVersions.battery.version[1] = deviceVersions.battery.version[2] = 0xffff;
	} else {
		for (i = 0; i < 3; i++) {
			if (!elmoReadVersion(deviceVersions.elmo))
				break;
			deviceVersions.elmo[1] = 0xff;
			deviceVersions.elmo[0] = 0xff;
		}
		for (i = 0; i < 3; i++) {
#if FW_SINCE(0x00090006)
			if (!cookieReadVersion(deviceVersions.cookie))
				break;
#else
			/* 0x00080504: the battery read here, half a second after the Cookie answers;
			   zeros where it does not */
			if (!cookieReadVersion(deviceVersions.cookie)) {
				powerSleep(500);
				if (batteryReadInfo(&deviceVersions.battery.id))
					deviceVersions.battery.id = deviceVersions.battery.version[0] =
						deviceVersions.battery.version[1] =
						deviceVersions.battery.version[2] = 0;
				break;
			}
#endif
			deviceVersions.cookie[1] = 0xff;
			deviceVersions.cookie[0] = 0xff;
			deviceVersions.battery.id = deviceVersions.battery.version[0] =
				deviceVersions.battery.version[1] =
				deviceVersions.battery.version[2] = 0xffff;
		}
	}
#if FW_SINCE(0x00090006)	/* 0x00080504 reads no Cookie status and leaves the charge alone */
#if FW_SINCE(0x00090203)
	if (deviceVersions.cookie[0] == 0x80) {
#else
	if (deviceVersions.cookie[1] != 0xff) {	/* 0x00090009: any Cookie that answered */
#endif
		cookieReadStatus1(&status);
		if (!(status & 0x02)) {
#if FW_SINCE(0x01000104)
			if ((status & 0xe0) == 5)	/* never true */
#else
			if ((status & 0xe0) == 0xa0)
#endif
				cookieCtrlBatteryCharge(0);
#if FW_SINCE(0x00090502)
			logEvent(0x0d);
#endif
		}
		for (i = 0; i < 6; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id)) {
				cookieReadStatus1(&status);
				if (status & 0x02) {
#if FW_SINCE(0x00090502)
					if (deviceVersions.battery.version[1] >= 0x408 && deviceVersions.battery.version[1] < 0x412)
						powerSleep(0x2a8);
#endif
					break;
				}
			}
			if (configGet()->start.bits.batteryNoRetry)
				break;
			powerSleep(500);
		}
	}
#if FW_SINCE(0x00090203)
	else if (deviceVersions.cookie[0] == 0) {
		for (i = 0; i < 3; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id))
				break;
		}
	}
#endif
	cookieCtrlBatteryCharge(1);
#endif
#if !FW_SINCE(0x01000104)
	elmoPowerOnStage = BOARD_IS(0x00411000) && !HW_IRT001 ? 0x0b : 7;
#endif
	chargeInit();
}

/* the regulators' levels, then the resume's Elmo steps; 0 or elmoWrite's error */
unsigned char powerSeqResume(void)
{
	unsigned char length;
	unsigned char result;
	unsigned char i;
#if !BOARD_102603 || FW_SINCE(0x00090006)	/* 0x00080504 has no such local */
	union PowerRequest mode;
#endif

	powerSeqLevels();
#if !FW_SINCE(0x0100060B)
	powerWaitTimers(1);
#endif
#if BOARD_P95
	P95 = 1;
#endif
	powerWaitInit();
	for (i = 0; i < boardResumeSteps(); i++) {
		length = elmoSetMasks((struct ElmoMasks *)&powerResumeSteps[i].masks, elmoCommand);
		result = elmoWrite(elmoCommand, length);
		if (result != 0)
			goto out;
		powerWaitMs(powerResumeSteps[i].delay);
	}
#if BOARD_102603
#if FW_SINCE(0x00090502)
	sleepFactorSet10(1);
#endif
#else
	mode = *stateGetWakeupFactor();
	if (mode.bits.mode == 0x0c)
		powerSdOn();
#endif
#if FW_SINCE(0x00090804)
	if (elmoStatusMask.reg[0] & 0x10)
		powerSeqWaitP67();
#endif
out:
	TT0L = 0x08;
	TMMK03 = 1;
	return result;
}

/* the Elmo's regulators 0x81-0x84 at the levels the settings give, trimmed */
void powerSeqLevels(void)
{
#if FW_SINCE(0x00090206)
	unsigned char trim;
	unsigned char level;
#else
	unsigned char level;
	unsigned char trim;
#endif

#if !FW_SINCE(0x01000104)
	/* before 0x01000104 the Elmo's version 0x80 gets register 0x38 set to 1 */
	if (deviceVersions.elmo[1] == 0x80) {
		elmoCommand[0] = 0x38;
		elmoCommand[1] = 0x01;
		elmoCommand[2] = 0x00;
		elmoWrite(elmoCommand, 3);
	} else
#endif
#if BOARD_IRS1001
	/* IRS-1001's Elmo: register 0x38 to 3 and 0x23 to 1 */
	elmoCommand[0] = 0x38;
	elmoCommand[1] = 0x03;
	elmoCommand[2] = 0x00;
	elmoWrite(elmoCommand, 3);
	elmoCommand[0] = 0x23;
	elmoCommand[1] = 0x01;
	elmoCommand[2] = 0x00;
	elmoWrite(elmoCommand, 3);
#else
	if (deviceVersions.elmo[1] == 0x90) {
		elmoCommand[0] = 0x50;
		elmoCommand[1] = 0xff;
		elmoCommand[2] = 0xc2;
		elmoCommand[3] = 0x56;
		elmoCommand[4] = 0x55;
		elmoCommand[5] = 0x00;
		elmoWrite(elmoCommand, 6);
	}
#if !BOARD_102603 && FW_SINCE(0x00090206)
	elmoCommand[0] = 0x0f;
	elmoCommand[1] = 0x01;
	elmoCommand[2] = 0x00;
	elmoWrite(elmoCommand, 3);
#endif
#endif
#if !FW_SINCE(0x00090206)
	/* 0x00090009 and 0x00090203: the levels and their trim from the parameters */
	paramGetBase(&trim);
	paramGetLevel0(&level);
	elmoCommand[0] = 0x81;
	elmoCommand[1] = levelTrim(level, trim);
	elmoCommand[2] = 0x00;
	paramGetLevel1(&level);
	elmoCommand[3] = 0x82;
	elmoCommand[4] = levelTrim(level, trim);
	elmoCommand[5] = 0x00;
	paramGetLevel2(&level);
	elmoCommand[6] = 0x83;
	elmoCommand[7] = levelTrim(level, trim);
	elmoCommand[8] = 0x00;
	paramGetLevel3(&level);
	elmoCommand[9] = 0x84;
	elmoCommand[10] = levelTrim(level, trim);
	elmoCommand[11] = 0x00;
#else
	trim = configGet()->voltageTrim;
	level = configGet()->voltage81;
	elmoCommand[0] = 0x81;
	elmoCommand[1] = levelTrim(level, trim);
	elmoCommand[2] = 0x00;
	level = configGet()->voltage82;
	elmoCommand[3] = 0x82;
	elmoCommand[4] = levelTrim(level, trim);
	elmoCommand[5] = 0x00;
	level = configGet()->voltage83;
	elmoCommand[6] = 0x83;
	elmoCommand[7] = levelTrim(level, trim);
	elmoCommand[8] = 0x00;
	level = configGet()->voltage84;
	elmoCommand[9] = 0x84;
	elmoCommand[10] = levelTrim(level, trim);
	elmoCommand[11] = 0x00;
#endif
	elmoWrite(elmoCommand, 12);
}

#if FW_SINCE(0x00090804)
/* P3.1 low until P6.7 goes low, at most six waits */
void powerSeqWaitP67(void)
{
#if !BOARD_WLAN_102603
	unsigned char i;

#if !FW_SINCE(0x01000104)
	if (!configGet()->CONFIG_WLAN_LINE)
		return;
#endif
	P31 = 0;
	for (i = 0; i < 6; i++) {
		powerWaitMs(5);
		if (!P6.7)
			break;
	}
	P31 = 1;
#endif
}
#endif

/* the lines low, the devices off, then the off sequence's Elmo steps */
unsigned char powerSeqOff(void)
{
	unsigned char length;
	unsigned char result;
	unsigned char i;

	P0.6 = 0;
	P0.5 = 0;
	P6.3 = 0;
	P13.1 = 0;
#if FW_SINCE(0x00090203)	/* 0x00090009 keeps the line, and stops the clock output last */
	POWER_SET_WLAN_LINE(0);
#endif
#if BOARD_102603 && !BOARD_WLAN_102603 && !HW_IRT001
	P6.7 = 0;
#endif
#if FW_SINCE(0x00090804) && BOARD_BBMC
	P37 = 0;
#endif
#if FW_SINCE(0x00090203)
	clockOutputStop();
#endif
#if FW_SINCE(0x00090007)	/* 0x00080504 has neither, 0x00090006 only powerRmrOff */
	powerMultiCnOff();
#endif
#if FW_SINCE(0x00090006)
	powerRmrOff();
#endif
	powerWaitInit();
	for (i = 0; i < boardOffSteps(); i++) {
		length = elmoClearMasks((struct ElmoMasks *)&powerOffSteps[i].masks, elmoCommand);
		result = elmoWrite(elmoCommand, length);
		if (result != 0)
			goto out;
		powerWaitMs(powerOffSteps[i].delay);
	}
#if BOARD_P95
	P95 = 0;
#endif
#if FW_SINCE(0x00090203)
	powerLedSet(0);
#endif
	sleepFactorSet0(0);
	sleepFactorSet2(0);
	sleepFactorSet1(0);
#if FW_SINCE(0x01000306)
	sleepFactorSet10(0);
#endif
#if !FW_SINCE(0x00090203)
	clockOutputStop();
#endif
out:
	TT0L = 0x08;
	TMMK03 = 1;
#if !FW_SINCE(0x0100060B)
	powerStartTimer(1, POWER_TIMER(4), 0x1c);
#endif
	return result;
}

/* as the off sequence, with the clock output and P9.5 kept while the Elmo's mask 0x10 is set */
unsigned char powerSeqSuspend(void)
{
	unsigned char length;
	unsigned char result;
	unsigned char i;

	P0.6 = 0;
	P0.5 = 0;
#if FW_SINCE(0x00090203)	/* 0x00090009 keeps the clock output */
	if (!(elmoStatusMask.reg[0] & 0x10))
		clockOutputStop();
#endif
#if FW_SINCE(0x0100060B)
	else
		P3.1 = 1;
#endif
#if FW_SINCE(0x00090007)	/* 0x00080504 has neither, 0x00090006 only powerRmrOff */
	powerMultiCnOff();
#endif
#if FW_SINCE(0x00090006)
	powerRmrOff();
#endif
	powerWaitInit();
	for (i = 0; i < boardSuspendSteps(); i++) {
		length = elmoClearMasks((struct ElmoMasks *)&powerSuspendSteps[i].masks, elmoCommand);
		result = elmoWrite(elmoCommand, length);
		if (result != 0)
			goto out;
		powerWaitMs(powerSuspendSteps[i].delay);
	}
#if BOARD_P95
	if (!(elmoStatusMask.reg[0] & 0x10))
		P95 = 0;
#endif
#if !FW_SINCE(0x00090203)
	/* 0x00090009 leaves the power LED */
#elif !FW_SINCE(0x00090804)
	powerLedSet(0);
#elif !FW_SINCE(0x01000104)
	if (!configGet()->suspendLedKeep) {
		if (powerLedGetStatus() & configGet()->suspendLedMask)
			powerLedFadeStart(1);
		else
#if BOARD_POWER_LED_ELMO
			powerLedSet(0);
#else
			powerLedFadeStart(0);
#endif
	}
#endif
	sleepFactorSet2(0);
#if FW_SINCE(0x01000306)
	sleepFactorSet1(0);
	sleepFactorSet10(0);
#endif
out:
	TT0L = 0x08;
	TMMK03 = 1;
#if !FW_SINCE(0x0100060B)
	powerStartTimer(1, POWER_TIMER(4), 0x1c);
#endif
	return result;
}

#if !FW_SINCE(0x00090006)	/* 0x00080504's order */
/* 0x0884: Elmo register 0xa0's bit 14 or bit 13 set (the second wins when both are asked);
   neither leaves the mask written unset */
unsigned char powerElmoA0Set(unsigned char bit14, unsigned char bit13)
{
	unsigned char result;
	unsigned int mask;

	powerWaitTimers(0x40);
	if (bit14)
		mask = elmoStatusMask.reg[0] | 0x4000;
	if (bit13)
		mask = elmoStatusMask.reg[0] | 0x2000;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerSleep(2);
	}
	return result;
}

/* and clear */
unsigned char powerElmoA0Clear(unsigned char bit14, unsigned char bit13)
{
	unsigned char result;
	unsigned int mask;

	powerWaitTimers(0x40);
	if (bit14)
		mask = elmoStatusMask.reg[0] & 0xbfff;
	if (bit13)
		mask = elmoStatusMask.reg[0] & 0xdfff;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerStartTimer(0x40, POWER_TIMER(0x0a), 0x1e);
	}
	return result;
}

#if BOARD_102603 && !HW_IS(0x00315200)	/* 0x00315000 has 0x00102603's */
/* 0x0885: Elmo register 0xa3's bits 15 and 14 and register 0xa0's bit 12 set, one per
   argument */
unsigned char powerElmoA0Set2(unsigned char bit15, unsigned char bit14, unsigned char bit12)
{
	unsigned char result;
	unsigned int mask3;
	unsigned int mask0;

	powerWaitTimers(0x80);
	if (bit15)
		mask3 = elmoStatusMask.reg[3] | 0x8000;
	if (bit14)
		mask3 = elmoStatusMask.reg[3] | 0x4000;
	if (bit12)
		mask0 = elmoStatusMask.reg[0] | 0x1000;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask0)[1];
	elmoCommand[2] = (unsigned char)mask0;
	elmoCommand[3] = 0xa3;
	elmoCommand[4] = ((unsigned char *)&mask3)[1];
	elmoCommand[5] = (unsigned char)mask3;
	result = elmoWrite(elmoCommand, 6);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask0;
		elmoStatusMask.reg[3] = mask3;
		powerSleep(2);
	}
	return result;
}

/* and clear */
unsigned char powerElmoA0Clear2(unsigned char bit15, unsigned char bit14, unsigned char bit12)
{
	unsigned char result;
	unsigned int mask3;
	unsigned int mask0;

	powerWaitTimers(0x80);
	if (bit15)
		mask3 = elmoStatusMask.reg[3] & 0x7fff;
	if (bit14)
		mask3 = elmoStatusMask.reg[3] & 0xbfff;
	if (bit12)
		mask0 = elmoStatusMask.reg[0] & 0xefff;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask0)[1];
	elmoCommand[2] = (unsigned char)mask0;
	elmoCommand[3] = 0xa3;
	elmoCommand[4] = ((unsigned char *)&mask3)[1];
	elmoCommand[5] = (unsigned char)mask3;
	result = elmoWrite(elmoCommand, 6);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask0;
		elmoStatusMask.reg[3] = mask3;
		powerStartTimer(0x80, POWER_TIMER(0x0b), 0x1e);
	}
	return result;
}
#else
/*
 * 0x0885: Elmo register 0xa0's bit 12 set; the other two are not used. 0x00080504 keeps
 * register 0xa3's mask too: the set computes into it, so register 0xa0 is written with mask
 * unset, and register 0xa3's copy takes mask3 as it was.
 */
unsigned char powerElmoA0Set2(unsigned char bit12, unsigned char unused1, unsigned char unused0)
{
	unsigned char result;
	unsigned int mask3;
	unsigned int mask;

	powerWaitTimers(0x80);
	if (bit12)
		mask3 = elmoStatusMask.reg[0] | 0x1000;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		elmoStatusMask.reg[3] = mask3;
		powerSleep(2);
	}
	return result;
}

/* and clear; register 0xa3's copy takes mask3 unset */
unsigned char powerElmoA0Clear2(unsigned char bit12, unsigned char unused1, unsigned char unused0)
{
	unsigned char result;
	unsigned int mask3;
	unsigned int mask;

	powerWaitTimers(0x80);
	if (bit12)
		mask = elmoStatusMask.reg[0] & 0xefff;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		elmoStatusMask.reg[3] = mask3;
		powerStartTimer(0x80, POWER_TIMER(0x0b), 0x1e);
	}
	return result;
}
#endif

/* 0x0886: HDMI CEC's power (Elmo register 0xa1's bit 15) */
unsigned char powerHdmiCecOn(void)
{
	unsigned char result;
#if BOARD_HDMI
	unsigned int mask;

	if (elmoStatusMask.reg[1] & 0x8000) {
		result = 0;
	} else {
		powerWaitTimers(2);
		mask = elmoStatusMask.reg[1] | 0x8000;
		elmoCommand[0] = 0xa1;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[1] = mask;
			powerSleep(3);
		}
	}
#else
	result = 0;
#endif
	return result;
}

unsigned char powerHdmiCecOff(void)
{
	unsigned char result;
#if BOARD_HDMI
	unsigned int mask;

	if (elmoStatusMask.reg[1] & 0x8000) {
		mask = elmoStatusMask.reg[1] & 0x7fff;
		elmoCommand[0] = 0xa1;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[1] = mask;
			powerStartTimer(2, POWER_TIMER(5), 0x32);
		}
	} else {
		result = 0;
	}
#else
	result = 0;
#endif
	return result;
}

/* 0x0887: the motion sensor's power (Elmo register 0xa2's bit 7) */
unsigned char powerMotionOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[2] & 0x0080) {
		result = 0;
	} else {
		powerWaitTimers(4);
		mask = elmoStatusMask.reg[2] | 0x0080;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
			powerSleep(3);
#if BOARD_MOTION
			if (BOARD_MOTION_READY)
				motionOpen();
			else
				motionSetAbsent();
#endif
		}
	}
	return result;
}

/* the mask is cleared before the write */
unsigned char powerMotionOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[2] & 0x0080) {
		P6.3 = 0;
#if BOARD_MOTION
		motionClose();
#endif
		mask = elmoStatusMask.reg[2] &= 0xff7f;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
			powerStartTimer(4, POWER_TIMER(6), 0x4b);
		}
	} else {
		result = 0;
	}
	return result;
}
#endif

#if BOARD_102603
/* 0x00102603's SD power: its own wait mask (8) and off timer (7, 0x25) */
unsigned char powerSdOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[0] & 0x0001) {
		result = 0;
	} else {
		powerWaitTimers(0x0008);
		mask = elmoStatusMask.reg[0] | 0x0001;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask;
#if FW_SINCE(0x00090502)
			timerStart(TIMER(0x12), 1);
#else
			powerSleep(5);
#endif
		}
	}
	return result;
}

unsigned char powerSdOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[0] & 0x0001) {
		mask = elmoStatusMask.reg[0] & 0xfffe;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask;
#if FW_SINCE(0x00090502)
			timerStop(TIMER(0x12));
			sleepFactorSet10(0);
#endif
			powerStartTimer(0x0008, POWER_TIMER(0x07), 0x25);
		}
	} else {
		result = 0;
	}
	return result;
}
#elif !FW_SINCE(0x0100060B)
/* a device this board does not have (not in 0x0100060B's device jobs) */
unsigned char powerNoneOn(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerNoneOff(void)
{
	unsigned char result;

	result = 0;
	return result;
}
#endif

/* the accessory port's power: the Elmo's mask 4 bit 15 */
unsigned char powerAccOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[4] & 0x8000) {
		result = 0;
	} else {
		powerWaitTimers(0x10);
		mask = elmoStatusMask.reg[4] | 0x8000;
		elmoCommand[0] = 0xa4;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[4] = mask;
#if FW_SINCE(0x00090502)
			timerStart(TIMER(0x11), 0);
#else
			sleepFactorSet2(1);
			powerSleep(2);
#endif
		}
	}
	return result;
}

unsigned char powerAccOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[4] & 0x8000) {
		mask = elmoStatusMask.reg[4] & 0x7fff;
		elmoCommand[0] = 0xa4;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[4] = mask;
#if FW_SINCE(0x00090502)
			timerStop(TIMER(0x11));
#endif
			sleepFactorSet2(0);
			powerStartTimer(0x10, POWER_TIMER(8), 0x4b);
		}
	} else {
		result = 0;
	}
	return result;
}

unsigned char powerOnWlan(void)
{
	unsigned char result;
	unsigned int mask0;
	unsigned int mask2;

	if (elmoStatusMask.reg[0] & 0x0010) {
		result = 0;
	} else {
		powerWaitTimers(0x20);
		clockOutputStart();
		powerSleep(1);
		mask0 = elmoStatusMask.reg[0] | 0x0010;
		mask2 = elmoStatusMask.reg[2] | 0x1000;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask0)[1];
		elmoCommand[2] = (unsigned char)mask0;
		elmoCommand[3] = 0xa2;
		elmoCommand[4] = ((unsigned char *)&mask2)[1];
		elmoCommand[5] = (unsigned char)mask2;
		result = elmoWrite(elmoCommand, 6);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask0;
			elmoStatusMask.reg[2] = mask2;
			sleepFactorSet0(1);
			powerSleep(2);
#if !BOARD_WLAN_102603
			POWER_SET_WLAN_LINE(1);
#endif
		}
	}
	return result;
}

unsigned char powerOffWlan(void)
{
	unsigned char result;
	unsigned int mask0;
	unsigned int mask2;

	if (elmoStatusMask.reg[0] & 0x0010) {
		P13.1 = 0;
#if !FW_SINCE(0x00090203)
		/* 0x00090009 keeps the line, and stops the clock output once the WLAN is off */
#if BOARD_102603 && !HW_IRT001
		P6.7 = 0;
#endif
#elif BOARD_WLAN_102603
		POWER_SET_WLAN_LINE(0);
		clockOutputStop();
#elif BOARD_102603 && !HW_IRT001
		P6.7 = 0;
		clockOutputStop();
		POWER_SET_WLAN_LINE(0);
#else
		clockOutputStop();
		POWER_SET_WLAN_LINE(0);
#endif
		mask0 = elmoStatusMask.reg[0] & 0xffef;
		mask2 = elmoStatusMask.reg[2] & 0xefff;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask0)[1];
		elmoCommand[2] = (unsigned char)mask0;
		elmoCommand[3] = 0xa2;
		elmoCommand[4] = ((unsigned char *)&mask2)[1];
		elmoCommand[5] = (unsigned char)mask2;
		result = elmoWrite(elmoCommand, 6);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask0;
			elmoStatusMask.reg[2] = mask2;
			sleepFactorSet0(0);
			powerStartTimer(0x20, POWER_TIMER(9), 0x64);
#if !FW_SINCE(0x00090203)
			clockOutputStop();	/* 0x00090009: once the WLAN is off */
#endif
#if FW_SINCE(0x01000104)
#if BOARD_P95
			if (stateGet() & 0x080c)
				P95 = 0;
#endif
#endif
		}
	} else {
		result = 0;
	}
	return result;
}

#if !FW_SINCE(0x00090203) && !BOARD_BBMC
/* 0x00090009 has the 3G modem's start here, before its cycle (powerBbmcStart below) */
unsigned char powerBbmcStart(void)
{
	unsigned char result;

	result = 4;	/* no 3G modem */
	return result;
}
#endif

#if !FW_SINCE(0x01000104)
/* the 3G modem's power off and on again (the off and the on of 0x01000104's powerBbmcCycle in
   one) */
unsigned char powerBbmcCycle(void)
{
	unsigned char result;

#if BOARD_BBMC
#if FW_SINCE(0x00090804)
	P37 = 0;
	P125 = 0;
#endif
	if (bbmcGetMode()) {
		bbmcSetOff();
#if BOARD_102603
		P33 = 1;
		powerSleep(0x0e74);
		P33 = 0;
#else
		P33 = 1;
		powerWaitInput(1, 0x4e2);
		P33 = 0;
#endif
		if (input2Bit0Get() == 0)
			return 0;
		return 2;
	}
	bbmcSetOff();
#if !FW_SINCE(0x00090804)
	P37 = 0;	/* 0x00090206 and 0x00090502: after the BBMC's off */
	P125 = 0;
#endif
	if (input2Bit0Get() == 1) {
		P6.5 = 1;
		powerSleep(0x14);
		P6.5 = 0;
		powerWaitInput(1, 0x4e2);
		if (input2Bit0Get()) {
			P33 = 1;
			powerSleep(0x0e74);
			P33 = 0;
		}
		powerSleep(0x20);
		if (input2Bit1Get() == 0) {
			sleepFactorSet9(0);
			result = 0;
		} else {
			result = 2;
		}
	} else {
		result = 0;
	}
#else
	result = 4;	/* no 3G modem */
#endif
	if (result)
		bbmcWake();
	return result;
}

/* at the off: the 3G modem's P3.3 pulse, then its state (2: up) */
unsigned char powerBbmcRestart(void)
{
	unsigned char result;

#if BOARD_BBMC
	bbmcSetOff();
	P37 = 0;
	P125 = 0;
	P33 = 1;
	powerSleep(0x0e74);
	P33 = 0;
	if (input2Bit1Get() == 0) {
		sleepFactorSet9(0);
		result = 0;
	} else {
		bbmcWake();
		result = 2;
	}
#else
	result = 4;	/* no 3G modem */
#endif
	return result;
}
#endif

#if FW_SINCE(0x00090006)	/* 0x00080504 has these last; no RMR or multi-connector power */
/* a regulator's levels: 0-0x3f, on IRS-1001 0x48-0x87 */
#if BOARD_IRS1001
#define POWER_LEVEL_MIN		0x48
#define POWER_LEVEL_MAX		0x87
#else
#define POWER_LEVEL_MIN		0
#define POWER_LEVEL_MAX		0x3f
#endif

/* a regulator's level (0x81-0x84); the result is not set for another register */
unsigned char powerSetVoltage(unsigned char regulator, unsigned char level)
{
	unsigned char result;

	if (regulator >= 0x81 && regulator < 0x85) {
		elmoCommand[0] = regulator;
#if FW_SINCE(0x0100060B)
		if (level < POWER_LEVEL_MIN)
			level = POWER_LEVEL_MIN;
		if (level > POWER_LEVEL_MAX)
			level = POWER_LEVEL_MAX;
		elmoCommand[1] = level;
#else
		elmoCommand[1] = level & 0x3f;
#endif
		elmoCommand[2] = 0;
		result = elmoWrite(elmoCommand, 3);
		powerSleep(2);
	}
	return result;
}

/* the Cookie's register 3's bit 7 set */
unsigned char powerCookieSet(void)
{
	unsigned char result;

#if FW_SINCE(0x00090203)	/* 0x00090009 writes the register it last read */
	cookieReadReg3();
#endif
#if BOARD_COOKIE_REG_HIGH && FW_SINCE(0x00090502)	/* 0x00090206: plain 3 */
	elmoCommand[0] = configGet()->cookieRegHigh ? 0xa3 : 0x03;
#else
	elmoCommand[0] = 0x03;
#endif
	elmoCommand[1] = cookieRegs[0].byte[0] | 0x80;
	elmoCommand[2] = cookieRegs[0].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[0].byte[0] = elmoCommand[1];
	return result;
}

/* and clear, unless the power status or chargeCookieHeld holds it */
unsigned char powerCookieClear(void)
{
	unsigned char result;

#if !FW_SINCE(0x00090203)
	{	/* 0x00090009: no lock, the register as last read */
#else
	result = 0;
#endif
#if !FW_SINCE(0x00090203)
#elif FW_SINCE(0x01000107)
	if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 && !chargeCookieHeld) {
#elif FW_SINCE(0x00090903) || !FW_SINCE(0x00090502)
	if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1) {
#else
	if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 && chargeVbus) {
#endif
#if FW_SINCE(0x00090203)
		cookieReadReg3();
#endif
#if BOARD_COOKIE_REG_HIGH && FW_SINCE(0x00090502)
		elmoCommand[0] = configGet()->cookieRegHigh ? 0xa3 : 0x03;
#else
		elmoCommand[0] = 0x03;
#endif
		elmoCommand[1] = cookieRegs[0].byte[0] & 0x7f;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = elmoCommand[1];
	}
	return result;
}

#if BOARD_102603
/* no RMR, no multi-connector power; and a device it does not have */
unsigned char powerRmrOn(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerRmrOff(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerNoneOn(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerNoneOff(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerMultiCnOn(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerMultiCnOff(void)
{
	unsigned char result;

	result = 0;
	return result;
}
#else
/* the RMR's power: the Elmo's register 0xaa bit 15, on IRS-1001 its register 0xa1 bit 13 */
#if BOARD_IRS1001
#define POWER_RMR_ELMO_REG	0xa1
#define POWER_RMR_REG		elmoStatusMask.reg[1]
#define POWER_RMR_BIT		0x2000
#else
#define POWER_RMR_ELMO_REG	0xaa
#define POWER_RMR_REG		elmoRegAA
#define POWER_RMR_BIT		0x8000
#endif

unsigned char powerRmrOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (POWER_RMR_REG & POWER_RMR_BIT) {
		result = 0;
	} else {
		powerWaitTimers(0x1000);
		mask = POWER_RMR_REG | POWER_RMR_BIT;
		elmoCommand[0] = POWER_RMR_ELMO_REG;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			POWER_RMR_REG = mask;
			powerSleep(0x0d);
		}
	}
	return result;
}

unsigned char powerRmrOff(void)
{
	unsigned char result;
	unsigned int mask;

#if FW_SINCE(0x01000306) || !FW_SINCE(0x00090804)
	if (POWER_RMR_REG & POWER_RMR_BIT) {
#endif
		mask = POWER_RMR_REG & ~POWER_RMR_BIT;
		elmoCommand[0] = POWER_RMR_ELMO_REG;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			POWER_RMR_REG = mask;
			powerStartTimer(0x1000, TIMER(0x0d), 0x4b);
		}
#if FW_SINCE(0x01000306) || !FW_SINCE(0x00090804)
	} else {
		result = 0;
	}
#endif
	return result;
}

unsigned char powerSdOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[0] & 0x0001) {
		result = 0;
	} else {
		powerWaitTimers(0x2000);
		mask = elmoStatusMask.reg[0] | 0x0001;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask;
#if FW_SINCE(0x00090502)
			timerStart(TIMER(0x12), 1);
#else
			powerSleep(5);
#endif
		}
	}
	return result;
}

unsigned char powerSdOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[0] & 0x0001) {
		mask = elmoStatusMask.reg[0] & 0xfffe;
		elmoCommand[0] = 0xa0;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[0] = mask;
#if FW_SINCE(0x00090502)
			timerStop(TIMER(0x12));
			sleepFactorSet10(0);
			powerStartTimer(0x2000, TIMER(0x0e), 0x11f);
#else
			powerStartTimer(0x2000, TIMER(0x0e), 0x25);
#endif
		}
	} else {
		result = 0;
	}
	return result;
}

unsigned char powerMultiCnOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoRegAB & 0x8000) {
		result = 0;
	} else {
		mask = elmoRegAB | 0x8000;
		elmoCommand[0] = 0xab;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoRegAB = mask;
			powerSleep(0x28);
		}
	}
	return result;
}

unsigned char powerMultiCnOff(void)
{
	unsigned char result;
	unsigned int mask;

#if !FW_SINCE(0x00090804)
	/* 0x00090206 and 0x00090502: only when it is on */
	if (elmoRegAB & 0x8000) {
#endif
		mask = elmoRegAB & 0x7fff;
		elmoCommand[0] = 0xab;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoRegAB = mask;
			powerSleep(0x64);
		}
#if !FW_SINCE(0x00090804)
	} else {
		result = 0;
	}
#endif
	return result;
}
#endif
#endif	/* FW_SINCE(0x00090006) */

#if FW_SINCE(0x00090804)
/* timer 0x11 (powerAccOn) */
void powerAccTimer(void)
{
	sleepFactorSet2(1);
}

/* timer 0x12 (powerSdOn) */
void powerSdTimer(void)
{
	sleepFactorSet10(1);
}

void powerSeqInit(void)
{
	struct TimerHandler handler;

	handler.handler = powerAccTimer;
	timerSetHandler(TIMER(0x11), &handler);
	handler.handler = powerSdTimer;
	timerSetHandler(TIMER(0x12), &handler);
}
#endif

#if FW_SINCE(0x01000104)
/* at the off and the suspend: the Cookie's register 3's bit 7 set for a while (timer 0x1e) */
void powerCookieHold(void)
{
	cookieReadReg3();
	if ((cookieStatus1.byte[0] & 0xe0) == 0x80 && !(cookieRegs[0].byte[0] & 0x80)) {
		cookieCtrlCharge(0);
#if FW_SINCE(0x01000107)
		chargeCookieHeld = 1;
#endif
		timerStart(TIMER_RTC(0x1e), 2);
#if FW_SINCE(0x01000107)
		return;
#endif
	}
#if FW_SINCE(0x01000107)
	chargeCookieHeld = 0;
#endif
}

/* task 5's, after timer 0x1e (powerCookieHold) */
void powerCookieRelease(void)
{
#if FW_SINCE(0x01000107)
	chargeCookieHeld = 0;
#endif
	chargeLedSetReleased(1);
	cookieCtrlCharge(1);
}

/* the 3G modem's power: at the suspend, both */
unsigned char powerBbmcCycle(void)
{
	powerBbmcOff();
	return powerBbmcOn();
}

unsigned char powerBbmcOff(void)
{
	unsigned char result;

	result = 0;
#if BOARD_BBMC
	P37 = 0;
	P125 = 0;
	bbmcSetOff();
	if (input2Bit0Get() == 1 && bbmcGetMode() == 0) {
		P6.5 = 1;
		powerSleep(0x14);
		P6.5 = 0;
	}
#else
	result = 4;	/* no 3G modem */
#endif
	return result;
}

unsigned char powerBbmcOn(void)
{
	unsigned char result;

#if BOARD_BBMC
	if (bbmcGetMode()) {
#if BOARD_102603
		powerBbmcPulse();
#else
		P33 = 1;
		powerWaitInput(1, 0x4e2);
		P33 = 0;
#endif
		if (input2Bit0Get() == 0)
			return 0;
		return 2;
	}
	if (input2Bit0Get() == 1) {
		powerWaitInput(1, configGet()->bbmcPowerDelay);
		if (input2Bit0Get())
			powerBbmcPulse();
		powerSleep(0x20);
		if (input2Bit1Get() == 0) {
			sleepFactorSet9(0);
			result = 0;
		} else {
			result = 2;
		}
	} else {
		result = 0;
	}
#else
	result = 4;	/* no 3G modem */
#endif
	if (result)
		bbmcWake();
	return result;
}

/* the 3G modem's P3.3 pulse, 0x0e74 long (none without the modem) */
void powerBbmcPulse(void)
{
#if BOARD_BBMC
	P33 = 1;
	powerSleep(0x0e74);
	P33 = 0;
#endif
}
#elif FW_SINCE(0x00090903)
/* task 5's (flag 0x0400): the Cookie's charge off for a second and on again, while its status
   has no bit 1 and register 3's bit 7 is clear */
void powerCookieCycle(void)
{
	if (!cookieStatus1.bits.status1 && !(cookieRegs[0].byte[0] & 0x80)) {
		cookieCtrlCharge(0);
		powerSleep(1000);
		cookieCtrlCharge(1);
	}
}
#endif

#if FW_SINCE(0x00090006)	/* 0x00080504 has these after powerSeqSuspend */
/* 0x0884: Elmo register 0xa0's bit 14 or bit 13 set (the second wins when both are asked);
   neither leaves the mask written unset */
unsigned char powerElmoA0Set(unsigned char bit14, unsigned char bit13)
{
	unsigned char result;
	unsigned int mask;

	powerWaitTimers(0x40);
	if (bit14)
		mask = elmoStatusMask.reg[0] | 0x4000;
	if (bit13)
		mask = elmoStatusMask.reg[0] | 0x2000;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerSleep(2);
	}
	return result;
}

/* and clear */
unsigned char powerElmoA0Clear(unsigned char bit14, unsigned char bit13)
{
	unsigned char result;
	unsigned int mask;

	powerWaitTimers(0x40);
	if (bit14)
		mask = elmoStatusMask.reg[0] & 0xbfff;
	if (bit13)
		mask = elmoStatusMask.reg[0] & 0xdfff;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerStartTimer(0x40, POWER_TIMER(0x0a), 0x1e);
	}
	return result;
}

#if BOARD_102603 && !HW_IS(0x00315200)	/* 0x00315000 has 0x00102603's */
/* 0x0885: Elmo register 0xa3's bits 15 and 14 and register 0xa0's bit 12 set, one per
   argument */
unsigned char powerElmoA0Set2(unsigned char bit15, unsigned char bit14, unsigned char bit12)
{
	unsigned char result;
	unsigned int mask3;
	unsigned int mask0;

	powerWaitTimers(0x80);
	if (bit15)
		mask3 = elmoStatusMask.reg[3] | 0x8000;
	if (bit14)
		mask3 = elmoStatusMask.reg[3] | 0x4000;
	if (bit12)
		mask0 = elmoStatusMask.reg[0] | 0x1000;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask0)[1];
	elmoCommand[2] = (unsigned char)mask0;
	elmoCommand[3] = 0xa3;
	elmoCommand[4] = ((unsigned char *)&mask3)[1];
	elmoCommand[5] = (unsigned char)mask3;
	result = elmoWrite(elmoCommand, 6);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask0;
		elmoStatusMask.reg[3] = mask3;
		powerSleep(2);
	}
	return result;
}

/* and clear */
unsigned char powerElmoA0Clear2(unsigned char bit15, unsigned char bit14, unsigned char bit12)
{
	unsigned char result;
	unsigned int mask3;
	unsigned int mask0;

	powerWaitTimers(0x80);
	if (bit15)
		mask3 = elmoStatusMask.reg[3] & 0x7fff;
	if (bit14)
		mask3 = elmoStatusMask.reg[3] & 0xbfff;
	if (bit12)
		mask0 = elmoStatusMask.reg[0] & 0xefff;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask0)[1];
	elmoCommand[2] = (unsigned char)mask0;
	elmoCommand[3] = 0xa3;
	elmoCommand[4] = ((unsigned char *)&mask3)[1];
	elmoCommand[5] = (unsigned char)mask3;
	result = elmoWrite(elmoCommand, 6);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask0;
		elmoStatusMask.reg[3] = mask3;
		powerStartTimer(0x80, POWER_TIMER(0x0b), 0x1e);
	}
	return result;
}
#else
/* 0x0885: Elmo register 0xa0's bit 12 set; the other two are not used */
unsigned char powerElmoA0Set2(unsigned char bit12, unsigned char unused1, unsigned char unused0)
{
	unsigned char result;
	unsigned int mask;

	powerWaitTimers(0x80);
	if (bit12)
		mask = elmoStatusMask.reg[0] | 0x1000;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerSleep(2);
	}
	return result;
}

/* and clear */
unsigned char powerElmoA0Clear2(unsigned char bit12, unsigned char unused1, unsigned char unused0)
{
	unsigned char result;
	unsigned int mask;

	powerWaitTimers(0x80);
	if (bit12)
		mask = elmoStatusMask.reg[0] & 0xefff;
	elmoCommand[0] = 0xa0;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoStatusMask.reg[0] = mask;
		powerStartTimer(0x80, POWER_TIMER(0x0b), 0x1e);
	}
	return result;
}
#endif

/* 0x0886: HDMI CEC's power (Elmo register 0xa1's bit 15) */
unsigned char powerHdmiCecOn(void)
{
	unsigned char result;
#if BOARD_HDMI
	unsigned int mask;

	if (elmoStatusMask.reg[1] & 0x8000) {
		result = 0;
	} else {
		powerWaitTimers(2);
		mask = elmoStatusMask.reg[1] | 0x8000;
		elmoCommand[0] = 0xa1;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[1] = mask;
			powerSleep(3);
		}
	}
#else
	result = 0;
#endif
	return result;
}

unsigned char powerHdmiCecOff(void)
{
	unsigned char result;
#if BOARD_HDMI
	unsigned int mask;

	if (elmoStatusMask.reg[1] & 0x8000) {
		mask = elmoStatusMask.reg[1] & 0x7fff;
		elmoCommand[0] = 0xa1;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[1] = mask;
			powerStartTimer(2, POWER_TIMER(5), 0x32);
		}
	} else {
		result = 0;
	}
#else
	result = 0;
#endif
	return result;
}

/* 0x0887: the motion sensor's power (Elmo register 0xa2's bit 7) */
unsigned char powerMotionOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[2] & 0x0080) {
		result = 0;
	} else {
		powerWaitTimers(4);
		mask = elmoStatusMask.reg[2] | 0x0080;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
			powerSleep(3);
#if BOARD_MOTION
			if (BOARD_MOTION_READY)
				motionOpen();
			else
				motionSetAbsent();
#endif
		}
	}
	return result;
}

/* the mask is cleared before the write */
unsigned char powerMotionOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[2] & 0x0080) {
		P6.3 = 0;
#if BOARD_MOTION
		motionClose();
#endif
		mask = elmoStatusMask.reg[2] &= 0xff7f;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
			powerStartTimer(4, POWER_TIMER(6), 0x4b);
		}
	} else {
		result = 0;
	}
	return result;
}
#endif

#if !FW_SINCE(0x0100060B)
/* more devices this board does not have */
unsigned char powerNone2On(void)
{
	unsigned char result;
#if !FW_SINCE(0x00090006)
	unsigned int mask;	/* 0x00080504 keeps the frame of the others */
#endif

	result = 0;
	return result;
}

unsigned char powerNone2Off(void)
{
	unsigned char result;
#if !FW_SINCE(0x00090006)
	unsigned int mask;	/* 0x00080504 keeps the frame of the others */
#endif

	result = 0;
	return result;
}

unsigned char powerNone3On(void)
{
	unsigned char result;
#if !FW_SINCE(0x00090006)
	unsigned int mask;	/* 0x00080504 keeps the frame of the others */
#endif

	result = 0;
	return result;
}

unsigned char powerNone3Off(void)
{
	unsigned char result;
#if !FW_SINCE(0x00090006)
	unsigned int mask;	/* 0x00080504 keeps the frame of the others */
#endif

	result = 0;
	return result;
}
#endif

/* 0x0899: the headphone remote's power (Elmo register 0xa2's bit 11) */
/* the headphone remote's power: the Elmo's register 0xa2 bit 11, on IRS-1001 its register 0xa3
   bit 9 */
#if BOARD_IRS1001
#define POWER_HPREMOTE_ELMO_REG	0xa3
#define POWER_HPREMOTE_REG	elmoStatusMask.reg[3]
#define POWER_HPREMOTE_BIT	0x0200
#else
#define POWER_HPREMOTE_ELMO_REG	0xa2
#define POWER_HPREMOTE_REG	elmoStatusMask.reg[2]
#define POWER_HPREMOTE_BIT	0x0800
#endif

unsigned char powerHpremoteOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (POWER_HPREMOTE_REG & POWER_HPREMOTE_BIT) {
		result = 0;
	} else {
		powerWaitTimers(0x800);
		mask = POWER_HPREMOTE_REG | POWER_HPREMOTE_BIT;
		elmoCommand[0] = POWER_HPREMOTE_ELMO_REG;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			POWER_HPREMOTE_REG = mask;
			sleepFactorSet1(1);
			powerSleep(4);
		}
	}
	return result;
}

unsigned char powerHpremoteOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (POWER_HPREMOTE_REG & POWER_HPREMOTE_BIT) {
		mask = POWER_HPREMOTE_REG & ~POWER_HPREMOTE_BIT;
		elmoCommand[0] = POWER_HPREMOTE_ELMO_REG;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			POWER_HPREMOTE_REG = mask;
			sleepFactorSet1(0);
			powerStartTimer(0x800, POWER_TIMER11, 0x19);
		}
	} else {
		result = 0;
	}
	return result;
}

#if !FW_SINCE(0x00090006)	/* 0x00080504's order */
/* a regulator's levels: 0-0x3f, on IRS-1001 0x48-0x87 */
#if BOARD_IRS1001
#define POWER_LEVEL_MIN		0x48
#define POWER_LEVEL_MAX		0x87
#else
#define POWER_LEVEL_MIN		0
#define POWER_LEVEL_MAX		0x3f
#endif

/* a regulator's level (0x81-0x84); the result is not set for another register */
unsigned char powerSetVoltage(unsigned char regulator, unsigned char level)
{
	unsigned char result;

	if (regulator >= 0x81 && regulator < 0x85) {
		elmoCommand[0] = regulator;
#if FW_SINCE(0x0100060B)
		if (level < POWER_LEVEL_MIN)
			level = POWER_LEVEL_MIN;
		if (level > POWER_LEVEL_MAX)
			level = POWER_LEVEL_MAX;
		elmoCommand[1] = level;
#else
		elmoCommand[1] = level & 0x3f;
#endif
		elmoCommand[2] = 0;
		result = elmoWrite(elmoCommand, 3);
		powerSleep(2);
	}
	return result;
}

/* the Cookie's register 3's bit 7 set */
unsigned char powerCookieSet(void)
{
	unsigned char result;

#if FW_SINCE(0x00090203)	/* 0x00090009 writes the register it last read */
	cookieReadReg3();
#endif
#if BOARD_COOKIE_REG_HIGH && FW_SINCE(0x00090502)	/* 0x00090206: plain 3 */
	elmoCommand[0] = configGet()->cookieRegHigh ? 0xa3 : 0x03;
#else
	elmoCommand[0] = 0x03;
#endif
	elmoCommand[1] = cookieRegs[0].byte[0] | 0x80;
	elmoCommand[2] = cookieRegs[0].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[0].byte[0] = elmoCommand[1];
	return result;
}

/* and clear, unless the power status or chargeCookieHeld holds it */
unsigned char powerCookieClear(void)
{
	unsigned char result;

#if !FW_SINCE(0x00090203)
	{	/* 0x00090009: no lock, the register as last read */
#else
	result = 0;
#endif
#if !FW_SINCE(0x00090203)
#elif FW_SINCE(0x01000107)
	if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 && !chargeCookieHeld) {
#elif FW_SINCE(0x00090903) || !FW_SINCE(0x00090502)
	if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1) {
#else
	if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 && chargeVbus) {
#endif
#if FW_SINCE(0x00090203)
		cookieReadReg3();
#endif
#if BOARD_COOKIE_REG_HIGH && FW_SINCE(0x00090502)
		elmoCommand[0] = configGet()->cookieRegHigh ? 0xa3 : 0x03;
#else
		elmoCommand[0] = 0x03;
#endif
		elmoCommand[1] = cookieRegs[0].byte[0] & 0x7f;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = elmoCommand[1];
	}
	return result;
}
#endif

#if FW_SINCE(0x00090203) || BOARD_BBMC
/* the 3G modem's start: its power line pulsed when it is off */
unsigned char powerBbmcStart(void)
{
	unsigned char result;

#if BOARD_BBMC
	bbmcStart();
	if (input2Bit0Get() == 0) {
		P6.5 = 1;
		powerWaitInput(2, 0x2ee);
		P6.5 = 0;
		if (input2Bit0Get() == 1 && input2Bit1Get() == 1) {
			sleepFactorSet9(1);
			result = 0;
		} else {
			bbmcStop();
			result = 2;
		}
	} else {
		result = 0;
	}
#else
	result = 4;	/* no 3G modem */
#endif
	return result;
}
#endif

#if !LED_REQUESTS && FW_SINCE(0x00090203)	/* 0x00090009 has no blink */
/* before 0x01000104 the charge LED's blink, on timer 0x18 (chargeLedBlinkExpire): register 3's
   bit 2 and mode 1 while it runs */
void chargeLedSetBlink(unsigned char on)
{
	if (on) {
		chargeLedBlinking = 1;
		chargeLedBlinkOn = 1;
		cookieCtrlReg3Bit2(1);
		cookieCtrlReg3Mode(1);
		timerStart(CHARGE_LED_TIMER, configGet()->paramSetting1);
	} else {
		timerStop(CHARGE_LED_TIMER);
		chargeLedBlinkOn = 0;
		cookieCtrlReg3Bit2(0);
		cookieCtrlReg3Mode(0);
		chargeLedBlinking = 0;
	}
}
#endif

#if FW_SINCE(0x00090203)
/* the Elmo's LED register's bit 15 */
unsigned char powerLedSet(unsigned char on)
{
	unsigned char result;
	unsigned int led;

	if (on == 1)
		led = elmoLed | 0x8000;
	else
		led = elmoLed & 0x7fff;
	elmoCommand[0] = 0xa5;
	elmoCommand[1] = ((unsigned char *)&led)[1];
	elmoCommand[2] = (unsigned char)led;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0)
		elmoLed = led;
	return result;
}
#endif

#if FW_SINCE(0x0100060B)
/* serial command 0x01b0: the Elmo's registers 0xb0-0xb2 as the last power cut read them */
void powerGetCutRegs(unsigned char *regs)
{
	memcpy(regs, powerCutRegs, 6);
}
#endif
#endif /* SOURCE_0103 */
