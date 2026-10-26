/*
 * The updater's power sequences: the application's (app/powerseq.c) from before its log, its
 * BBMC and its LED, with timers 0x13 and 0x14, P3.1 driven only where the settings' wlanLine
 * says so, and the BBMC's start in the older form, its calls the updater's stubs.
 */
#pragma sfr

#include "ernie.h"
#include "battery.h"
#include "board.h"
#include "charge.h"
#include "chip.h"
#include "clockout.h"
#include "config.h"
#include "cookie.h"
#include "elmo.h"
#include "iica.h"
#include "powerseq.h"
#include "sleepfactor.h"
#include "state.h"
#include "timer.h"
#include "updater.h"

unsigned char powerSeqOnState;		/* 7 once powerSeqOn ran; read nowhere */

static const struct PowerStep __far powerResumeSteps[3] = {
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 6 },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 4 },
	{ { 0x7102, 0x0000, 0x0340, 0x0000, 0x0000, 0x0000 }, 8 },
};
static const struct PowerStep __far powerOffSteps[3] = {
	{ { 0x7113, 0x8000, 0x1bc0, 0x0000, 0x8000, 0x0000 }, 0x5c },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x70 },
};
static const struct PowerStep __far powerSuspendSteps[3] = {
	{ { 0x7103, 0x8000, 0x0bc0, 0x0000, 0x8000, 0x0000 }, 0x5c },
	{ { 0x8000, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 0x70 },
};

/* the versions and the battery's identity (0xff and 0xffff where none answered) */
void powerSeqOn(void)
{
	unsigned char i;
	unsigned char unused;
	unsigned char status;

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
			if (!cookieReadVersion(deviceVersions.cookie))
				break;
			deviceVersions.cookie[1] = 0xff;
			deviceVersions.cookie[0] = 0xff;
			deviceVersions.battery.id = deviceVersions.battery.version[0] =
				deviceVersions.battery.version[1] =
				deviceVersions.battery.version[2] = 0xffff;
		}
	}
	if (deviceVersions.cookie[0] == 0x80) {
		for (i = 0; i < 6; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id)) {
				cookieReadStatus1(&status);
				if (status & 0x02) {
					if (deviceVersions.battery.version[1] >= 0x408 && deviceVersions.battery.version[1] < 0x412)
						powerSleep2(0x2a8);
					break;
				}
			}
			if (configGet()->start.bits.batteryNoRetry)
				break;
			powerSleep2(500);
		}
	} else if (deviceVersions.cookie[0] == 0) {
		for (i = 0; i < 3; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id))
				break;
		}
	}
	cookieCtrlBatteryCharge(1);
	powerSeqOnState = 7;
	chargeInit();
}

/* the regulators' levels, then the resume's Elmo steps; 0 or elmoWrite's error */
unsigned char powerSeqResume(void)
{
	unsigned char length;
	unsigned char result;
	unsigned char i;
	union PowerRequest mode;

	powerSeqLevels();
	powerWaitTimers(1);
	P95 = 1;
	powerWaitInit();
	for (i = 0; i < boardResumeSteps(); i++) {
		length = elmoSetMasks((struct ElmoMasks *)&powerResumeSteps[i].masks, elmoCommand);
		result = elmoWrite(elmoCommand, length);
		if (result != 0)
			goto out;
		powerSleep(powerResumeSteps[i].delay);
	}
	mode = *stateGetWakeupFactor();
	if (mode.bits.mode == 0x0c)
		powerSdOn();
	if (elmoStatusMask.reg[0] & 0x10)
		powerSeqWaitP67();
out:
	TT0L = 0x08;
	TMMK03 = 1;
	return result;
}

/* the Elmo's regulators 0x81-0x84 at the levels the settings give, trimmed; an Elmo 0x80 or 0x90
   first set up */
void powerSeqLevels(void)
{
	unsigned char trim;
	unsigned char level;

	if (deviceVersions.elmo[1] == 0x80) {
		elmoCommand[0] = 0x38;
		elmoCommand[1] = 0x01;
		elmoCommand[2] = 0x00;
		elmoWrite(elmoCommand, 3);
	} else if (deviceVersions.elmo[1] == 0x90) {
		elmoCommand[0] = 0x50;
		elmoCommand[1] = 0xff;
		elmoCommand[2] = 0xc2;
		elmoCommand[3] = 0x56;
		elmoCommand[4] = 0x55;
		elmoCommand[5] = 0x00;
		elmoWrite(elmoCommand, 6);
	}
	elmoCommand[0] = 0x0f;
	elmoCommand[1] = 0x01;
	elmoCommand[2] = 0x00;
	elmoWrite(elmoCommand, 3);
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
	elmoWrite(elmoCommand, 12);
}

/* where the WLAN's line is driven: P3.1 low until P6.7 goes low, at most six waits */
void powerSeqWaitP67(void)
{
	unsigned char i;

	if (configGet()->wlanLine) {
		P31 = 0;
		for (i = 0; i < 6; i++) {
			powerSleep(5);
			if (!P6.7)
				break;
		}
		P31 = 1;
	}
}

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
	if (configGet()->wlanLine)
		P31 = 0;
	clockOutputStop();
	powerMultiCnOff();
	powerRmrOff();
	powerWaitInit();
	for (i = 0; i < boardOffSteps(); i++) {
		length = elmoClearMasks((struct ElmoMasks *)&powerOffSteps[i].masks, elmoCommand);
		result = elmoWrite(elmoCommand, length);
		if (result != 0)
			goto out;
		powerSleep(powerOffSteps[i].delay);
	}
	P95 = 0;
	sleepFactorSet0(0);
	sleepFactorSet2(0);
	sleepFactorSet1(0);
out:
	TT0L = 0x08;
	TMMK03 = 1;
	powerStartTimer(1, 4, 0x1c);
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
	if (!(elmoStatusMask.reg[0] & 0x10))
		clockOutputStop();
	powerMultiCnOff();
	powerRmrOff();
	powerWaitInit();
	for (i = 0; i < boardSuspendSteps(); i++) {
		length = elmoClearMasks((struct ElmoMasks *)&powerSuspendSteps[i].masks, elmoCommand);
		result = elmoWrite(elmoCommand, length);
		if (result != 0)
			goto out;
		powerSleep(powerSuspendSteps[i].delay);
	}
	if (!(elmoStatusMask.reg[0] & 0x10))
		P95 = 0;
	sleepFactorSet2(0);
out:
	TT0L = 0x08;
	TMMK03 = 1;
	powerStartTimer(1, 4, 0x1c);
	return result;
}

/* a device this board does not have */
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
			timerStart(0x13, 0);
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
			timerStop(0x13);
			sleepFactorSet2(0);
			powerStartTimer(0x10, 8, 0x4b);
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
		powerSleep2(1);
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
			powerSleep2(2);
			if (configGet()->wlanLine)
				P31 = 1;
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
		clockOutputStop();
		if (configGet()->wlanLine)
			P31 = 0;
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
			powerStartTimer(0x20, 9, 0x64);
		}
	} else {
		result = 0;
	}
	return result;
}

/* the 3G modem's power, as the application's powerBbmcOff and powerBbmcOn before they were
   two */
unsigned char powerBbmcOn(void)
{
	unsigned char result;

	if ((unsigned char)sub_4E25()) {
		sub_4E21();
		P33 = 1;
		powerWaitInput(1, 0x4e2);
		P33 = 0;
		if ((unsigned char)sub_4D3F() == 0)
			return 0;
		return 2;
	}
	sub_4E21();
	P37 = 0;
	P125 = 0;
	if ((unsigned char)sub_4D3F() == 1) {
		P6.5 = 1;
		powerSleep2(0x14);
		P6.5 = 0;
		powerWaitInput(1, 0x4e2);
		if ((unsigned char)sub_4D3F()) {
			P33 = 1;
			powerSleep2(0x0e74);
			P33 = 0;
		}
		powerSleep2(0x20);
		if ((unsigned char)sub_4D47() == 0) {
			sleepFactorSet9(0);
			result = 0;
		} else {
			result = 2;
		}
	} else {
		result = 0;
	}
	if (result)
		sub_4E24();
	return result;
}

/* the 3G modem off, then on by its P3.3 pulse */
unsigned char powerBbmcCycle(void)
{
	unsigned char result;

	sub_4E21();
	P37 = 0;
	P125 = 0;
	P33 = 1;
	powerSleep2(0x0e74);
	P33 = 0;
	if ((unsigned char)sub_4D47() == 0) {
		sleepFactorSet9(0);
		result = 0;
	} else {
		sub_4E24();
		result = 2;
	}
	return result;
}

/* a regulator's level (0x81-0x84); the result is not set for another register */
unsigned char powerSetVoltage(unsigned char regulator, unsigned char level)
{
	unsigned char result;

	if (regulator >= 0x81 && regulator < 0x85) {
		elmoCommand[0] = regulator;
		elmoCommand[1] = level & 0x3f;
		elmoCommand[2] = 0;
		result = elmoWrite(elmoCommand, 3);
		powerSleep2(2);
	}
	return result;
}

/* the Cookie's register 3's bit 7 set */
unsigned char powerCookieSet(void)
{
	unsigned char result;

	elmoCommand[0] = 0x03;
	elmoCommand[1] = cookieRegs[0].byte[0] | 0x80;
	elmoCommand[2] = cookieRegs[0].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[0].byte[0] = elmoCommand[1];
	return result;
}

/* and clear, unless the power status holds it; only while chargeVbus is set */
unsigned char powerCookieClear(void)
{
	unsigned char result;

	result = 0;
	if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 && chargeVbus) {
		elmoCommand[0] = 0x03;
		elmoCommand[1] = cookieRegs[0].byte[0] & 0x7f;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = elmoCommand[1];
	}
	return result;
}

unsigned char powerRmrOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoRegAA & 0x8000) {
		result = 0;
	} else {
		powerWaitTimers(0x1000);
		mask = elmoRegAA | 0x8000;
		elmoCommand[0] = 0xaa;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoRegAA = mask;
			powerSleep2(0x0d);
		}
	}
	return result;
}

/* whatever the RMR's state */
unsigned char powerRmrOff(void)
{
	unsigned char result;
	unsigned int mask;

	mask = elmoRegAA & 0x7fff;
	elmoCommand[0] = 0xaa;
	elmoCommand[1] = ((unsigned char *)&mask)[1];
	elmoCommand[2] = (unsigned char)mask;
	result = elmoWrite(elmoCommand, 3);
	if (result == 0) {
		elmoRegAA = mask;
		powerStartTimer(0x1000, 0x0f, 0x4b);
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
			timerStart(0x14, 1);
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
			timerStop(0x14);
			sleepFactorSet10(0);
			powerStartTimer(0x2000, 0x10, 0x11f);
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
			powerSleep2(0x28);
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
		powerSleep2(0x64);
	}
	return result;
}

/* timer 0x13 (powerAccOn) */
void powerAccTimer(void)
{
	sleepFactorSet2(1);
}

/* timer 0x14 (powerSdOn) */
void powerSdTimer(void)
{
	sleepFactorSet10(1);
}

void powerSeqInit(void)
{
	struct TimerHandler handler;

	handler.handler = powerAccTimer;
	timerSetHandler(0x13, &handler);
	handler.handler = powerSdTimer;
	timerSetHandler(0x14, &handler);
}
