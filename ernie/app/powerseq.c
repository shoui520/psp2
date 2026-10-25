/*
 * The power sequences' board parts (DEM-3000L): the devices read at the power-on, the Elmo
 * mask steps of the resume, off and suspend sequences with what goes around them, and the
 * devices' power.
 */
#pragma sfr

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
#include "motion.h"
#include "powerreq.h"
#include "powerseq.h"
#include "powerwait.h"
#include "serial2.h"
#include "sleepfactor.h"
#include "state.h"
#include "timer.h"

extern void sub_2DAE1(int);
extern void sub_2E03A(unsigned int);
extern unsigned char sub_11D57(void);
extern unsigned char sub_11D61(void);

struct DeviceVersions deviceVersions;
unsigned char elmoCommand[12];

static const struct PowerStep powerResumeSteps[3] = {
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0xa000 }, 6 },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 4 },
	{ { 0x7102, 0x0000, 0x0340, 0x0000, 0x0000, 0x0000 }, 8 },
};
static const struct PowerStep powerOffSteps[3] = {
	{ { 0x7113, 0x8000, 0x1bc0, 0x0000, 0x8000, 0x0000 }, 0x5c },
	{ { 0x8880, 0x0000, 0x0000, 0x1400, 0x0000, 0x0000 }, 0x5c },
	{ { 0x0004, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000 }, 0x70 },
};
static const struct PowerStep powerSuspendSteps[3] = {
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
		cookieReadStatus1(&status);
		if (!(status & 0x02)) {
			if ((status & 0xe0) == 5)
				cookieCtrlBatteryCharge(0);
			sub_2DAE1(0x0d);
		}
		for (i = 0; i < 6; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id)) {
				cookieReadStatus1(&status);
				if (status & 0x02) {
					if (deviceVersions.battery.version[1] >= 0x408 && deviceVersions.battery.version[1] < 0x412)
						powerSleep(0x2a8);
					break;
				}
			}
			if (configGet()->start.bits.batteryNoRetry)
				break;
			powerSleep(500);
		}
	} else if (deviceVersions.cookie[0] == 0) {
		for (i = 0; i < 3; i++) {
			if (!batteryReadInfo(&deviceVersions.battery.id))
				break;
		}
	}
	cookieCtrlBatteryCharge(1);
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
		powerWaitMs(powerResumeSteps[i].delay);
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

/* the Elmo's regulators 0x81-0x84 at the levels the settings give, trimmed */
void powerSeqLevels(void)
{
	unsigned char trim;
	unsigned char level;

	if (deviceVersions.elmo[1] == 0x90) {
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

/* P3.1 low until P6.7 goes low, at most six waits */
void powerSeqWaitP67(void)
{
	unsigned char i;

	P31 = 0;
	for (i = 0; i < 6; i++) {
		powerWaitMs(5);
		if (!P6.7)
			break;
	}
	P31 = 1;
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
	P31 = 0;
	P37 = 0;
	clockOutputStop();
	powerMultiCnOff();
	powerRmrOff();
	powerWaitInit();
	for (i = 0; i < boardOffSteps(); i++) {
		length = elmoClearMasks((struct ElmoMasks *)&powerOffSteps[i].masks, elmoCommand);
		result = elmoWrite(elmoCommand, length);
		if (result != 0)
			goto out;
		powerWaitMs(powerOffSteps[i].delay);
	}
	P95 = 0;
	powerLedSet(0);
	sleepFactorSet0(0);
	sleepFactorSet2(0);
	sleepFactorSet1(0);
	sleepFactorSet10(0);
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
		powerWaitMs(powerSuspendSteps[i].delay);
	}
	if (!(elmoStatusMask.reg[0] & 0x10))
		P95 = 0;
	sleepFactorSet2(0);
	sleepFactorSet1(0);
	sleepFactorSet10(0);
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
			timerStart(0x11, 0);
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
			timerStop(0x11);
			sleepFactorSet2(0);
			powerStartTimer(0x10, 8, 0x4b);
		}
	} else {
		result = 0;
	}
	return result;
}

unsigned char powerWirelessOn(void)
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
			P31 = 1;
		}
	}
	return result;
}

unsigned char powerWirelessOff(void)
{
	unsigned char result;
	unsigned int mask0;
	unsigned int mask2;

	if (elmoStatusMask.reg[0] & 0x0010) {
		P13.1 = 0;
		clockOutputStop();
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
			if (stateGet() & 0x080c)
				P95 = 0;
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
		elmoCommand[1] = level & 0x3f;
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

	cookieReadReg3();
	elmoCommand[0] = configGet()->cookieRegHigh ? 0xa3 : 0x03;
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

	result = 0;
	if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 && !chargeCookieHeld) {
		cookieReadReg3();
		elmoCommand[0] = configGet()->cookieRegHigh ? 0xa3 : 0x03;
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
			powerSleep(0x0d);
		}
	}
	return result;
}

unsigned char powerRmrOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoRegAA & 0x8000) {
		mask = elmoRegAA & 0x7fff;
		elmoCommand[0] = 0xaa;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoRegAA = mask;
			powerStartTimer(0x1000, 0x0d, 0x4b);
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
			timerStart(0x12, 1);
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
			timerStop(0x12);
			sleepFactorSet10(0);
			powerStartTimer(0x2000, 0x0e, 0x11f);
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
	timerSetHandler(0x11, &handler);
	handler.handler = powerSdTimer;
	timerSetHandler(0x12, &handler);
}

/* at the off and the suspend: the Cookie's register 3's bit 7 set for a while (timer 0x1e) */
void powerCookieHold(void)
{
	cookieReadReg3();
	if ((cookieStatus1.byte[0] & 0xe0) == 0x80 && !(cookieRegs[0].byte[0] & 0x80)) {
		cookieCtrlCharge(0);
		chargeCookieHeld = 1;
		timerStart(0x1e, 2);
		return;
	}
	chargeCookieHeld = 0;
}

/* task 5's, after timer 0x1e (powerCookieHold) */
void powerCookieRelease(void)
{
	chargeCookieHeld = 0;
	sub_2E03A(1);
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
	P37 = 0;
	P125 = 0;
	bbmcSetOff();
	if (sub_11D57() == 1 && bbmcGetMode() == 0) {
		P6.5 = 1;
		powerSleep(0x14);
		P6.5 = 0;
	}
	return result;
}

unsigned char powerBbmcOn(void)
{
	unsigned char result;

	if (bbmcGetMode()) {
		P33 = 1;
		powerWaitInput(1, 0x4e2);
		P33 = 0;
		if (sub_11D57() == 0)
			return 0;
		return 2;
	}
	if (sub_11D57() == 1) {
		powerWaitInput(1, configGet()->bbmcPowerDelay);
		if (sub_11D57())
			powerBbmcPulse();
		powerSleep(0x20);
		if (sub_11D61() == 0) {
			sleepFactorSet9(0);
			result = 0;
		} else {
			result = 2;
		}
	} else {
		result = 0;
	}
	if (result)
		bbmcWake();
	return result;
}

/* the 3G modem's P3.3 pulse, 0x0e74 long */
void powerBbmcPulse(void)
{
	P33 = 1;
	powerSleep(0x0e74);
	P33 = 0;
}

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
		powerStartTimer(0x40, 0x0a, 0x1e);
	}
	return result;
}

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
		powerStartTimer(0x80, 0x0b, 0x1e);
	}
	return result;
}

/* 0x0886: HDMI CEC's power (Elmo register 0xa1's bit 15) */
unsigned char powerHdmiCecOn(void)
{
	unsigned char result;
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
	return result;
}

unsigned char powerHdmiCecOff(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[1] & 0x8000) {
		mask = elmoStatusMask.reg[1] & 0x7fff;
		elmoCommand[0] = 0xa1;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[1] = mask;
			powerStartTimer(2, 5, 0x32);
		}
	} else {
		result = 0;
	}
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
			if (P7.2)
				motionOpen();
			else
				motionSetAbsent();
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
		motionClose();
		mask = elmoStatusMask.reg[2] &= 0xff7f;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
			powerStartTimer(4, 6, 0x4b);
		}
	} else {
		result = 0;
	}
	return result;
}

/* more devices this board does not have */
unsigned char powerNone2On(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerNone2Off(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerNone3On(void)
{
	unsigned char result;

	result = 0;
	return result;
}

unsigned char powerNone3Off(void)
{
	unsigned char result;

	result = 0;
	return result;
}

/* 0x0899: the headphone remote's power (Elmo register 0xa2's bit 11) */
unsigned char powerHpremoteOn(void)
{
	unsigned char result;
	unsigned int mask;

	if (elmoStatusMask.reg[2] & 0x0800) {
		result = 0;
	} else {
		powerWaitTimers(0x800);
		mask = elmoStatusMask.reg[2] | 0x0800;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
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

	if (elmoStatusMask.reg[2] & 0x0800) {
		mask = elmoStatusMask.reg[2] & 0xf7ff;
		elmoCommand[0] = 0xa2;
		elmoCommand[1] = ((unsigned char *)&mask)[1];
		elmoCommand[2] = (unsigned char)mask;
		result = elmoWrite(elmoCommand, 3);
		if (result == 0) {
			elmoStatusMask.reg[2] = mask;
			sleepFactorSet1(0);
			powerStartTimer(0x800, 0x0c, 0x19);
		}
	} else {
		result = 0;
	}
	return result;
}

/* the 3G modem's start: its power line pulsed when it is off */
unsigned char powerBbmcStart(void)
{
	unsigned char result;

	bbmcStart();
	if (sub_11D57() == 0) {
		P6.5 = 1;
		powerWaitInput(2, 0x2ee);
		P6.5 = 0;
		if (sub_11D57() == 1 && sub_11D61() == 1) {
			sleepFactorSet9(1);
			result = 0;
		} else {
			bbmcStop();
			result = 2;
		}
	} else {
		result = 0;
	}
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
