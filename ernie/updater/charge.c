/*
 * The charge, as the updater has it (an older revision of the application's): the Cookie's
 * status registers, read at each update (timer 0x18), make the power status and the charge's
 * sleep factors; the battery's readings while one is in; the system transitions go to the
 * updater's events (updStateEvent).
 */
#pragma sfr

#include "ernie.h"
#include "battery.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "param.h"
#include "powerseq.h"
#include "sleepfactor.h"
#include "state.h"
#include "timer.h"
#include "updater.h"

__sreg union CookieStatus1 cookieStatus1;
__sreg union CookieStatus2 cookieStatus2;
__sreg union PowerStatus powerStatus;
__sreg unsigned char chargeFactors;
__sreg unsigned char chargeControl;
__sreg struct ChargeTemp chargeTemp;
__sreg unsigned char chargeCurrent;
__sreg unsigned char chargeUsbMode;
__sreg unsigned char chargeVbus;
__sreg unsigned char chargeBbmcVersion;

/* cookieCtrlSupplyCurrentUsb's charge settings by the USB status and chargeUsbMode */
static const unsigned char __far chargeCurrents[4][5] = {
	{ 0x10, 0x10, 0x10, 0x10, 0xff },
	{ 0x10, 0x10, 0x10, 0x50, 0xff },
	{ 0x10, 0xc0, 0xc0, 0xc0, 0xc0 },
	{ 0x10, 0xc0, 0xc0, 0xc0, 0xc0 },
};

static struct ChargeLevel chargeLevels[10];
struct Charge charge;

void chargeInit(void)
{
	struct TimerHandler handler;

	cookieStatus1.word = 0;
	cookieStatus2.word = 0;
	powerStatus.word = 0;
	chargeTemp.setting1 = 0x50;
	chargeTemp.setting2 = 0;
	chargeTemp.level = 0xff;
	chargeTemp.readLast = 0xff;
	chargeTemp.read = 0xff;
	chargeTemp.temp = 0;
	chargeCurrent = 0x10;
	chargeUsbMode = 0;
	chargeVbus = 1;
	chargeBbmcVersion = 0;
	chargeControl.0 = 0;
	chargeControl.1 = 1;
	chargeFactors.0 = 0;
	if (deviceVersions.cookie[0] != 0x80) {
		charge.batteryVolt = configGet()->batteryVolt;
		charge.batteryRemainCap = configGet()->batteryRemainCap;
		charge.batteryLifePercent = configGet()->batteryLifePercent;
		charge.batteryTemp = configGet()->batteryTemp;
		charge.batteryBicTemp = configGet()->batteryBicTemp;
		powerStatus.byte[0] |= 0x60;
		charge.flagsLast = 0;
		charge.flagsRead = 0;
		charge.flags = 0;
		sleepFactorSetStatus3(1);
		sleepFactorSetStatus4(0);
	} else {
		chargeResetBattery();
		sleepFactorSetStatus3(0);
		sleepFactorSetStatus4(0);
		if (deviceVersions.cookie[1] < 0x31)
			cookieCtrlSupplyCurrentAc(0x60);
		if (deviceVersions.battery.id != 0) {
			batteryReadRemainCap(&charge.batteryRemainCap);
			batteryReadVolt(&charge.batteryVolt);
			charge.flagsLast = chargeReadFlags();
		}
		if (deviceVersions.battery.version[2] != 0)
			chargeSetBattery();
	}
	handler.handler = chargeTimer;
	timerSetHandler(0x18, &handler);
}

/* the transition goes to the updater's events */
void chargeTransition(int transition)
{
	struct StateEvent event;

	event.transition = transition;
	event.wakeMask = stateGetWakeMask();
	updStateEvent(&event);
}

/* the updater's event 0x82 (chargeTimer) */
void chargeUpdate(void)
{
	unsigned char reg;
	unsigned char ac;
	union CookieStatus1 status1;
	union CookieStatus2 status2;
	unsigned int changed;
	unsigned int state;

	if (deviceVersions.cookie[0] != 0x80)
		return;
	status1 = cookieStatus1;
	status2 = cookieStatus2;
	reg = 1;
	cookieRead(&reg, cookieStatus1.byte);
	reg = 2;
	cookieRead(&reg, cookieStatus2.byte);
	state = stateGet();
	ac = chargeFactors.0;
	if (status1.word != cookieStatus1.word || status2.word != cookieStatus2.word) {
		powerStatus.bits.charging = cookieStatus1.bits.charging;
		powerStatus.bits.battery = cookieStatus1.bits.battery;
		powerStatus.bits.ac = cookieStatus1.bits.ac;
		powerStatus.bits.status3 = cookieStatus1.bits.status3;
		powerStatus.bits.status2 = cookieStatus1.bits.status2;
		powerStatus.bits.status1 = cookieStatus1.bits.status1;
		powerStatus.bits.level = cookieStatus1.bits.level;
		powerStatus.bits.usb = cookieStatus2.bits.usb;
		chargeFactors.0 = powerStatus.bits.ac ? 1 : (cookieStatus2.bits.usb & 2) ? 1 : 0;
		sleepFactorSetStatus3(chargeFactors.0);
		if (chargeFactors.0 != ac) {
			if (chargeFactors.0) {
				charge.acDelay = 10;
				if (!(stateGetWakeMask() & 0x00400000))
					chargeControl.0 = 1;
			} else {
				chargeControl.0 = 0;
				charge.acDelay = 0x3c;
			}
			timerStart(0x18, charge.acDelay);
		}
		if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && (cookieStatus1.byte[1] & 0xc0) == 0x80)
			chargeFactors.1 = 1;
		else
			chargeFactors.1 = 0;
		sleepFactorSetStatus4(chargeFactors.1);
		if (cookieStatus2.bits.vbusCharge) {
			if (chargeUsbMode == 0 && !P14.5 && (cookieStatus2.bits.usb & 2)) {
				chargeUsbMode = 1;
				chargeSetCurrent();
			}
		} else if (status2.word & 0x8000) {
			if (deviceVersions.cookie[1] < 0x31)
				cookieCtrlSupplyCurrentUsb(0x10);
		}
		if (cookieStatus1.bits.level)
			;
	}
	if (deviceVersions.battery.id == 0) {
		if (batteryReadInfo(&deviceVersions.battery.id) == 0) {
			chargeSetBattery();
		} else {
			deviceVersions.battery.id = deviceVersions.battery.version[0] =
				deviceVersions.battery.version[1] = deviceVersions.battery.version[2] = 0;
			return;
		}
	} else {
		batteryReadTemp(&charge.batteryTemp);
		batteryReadBicTemp(&charge.batteryBicTemp);
		chargeTempUpdate(charge.batteryTemp, &chargeTemp);
	}
	if (powerStatus.bits.battery) {
		if (!(status1.word & 0x0002) && deviceVersions.battery.version[1] >= 0x0408 &&
		    deviceVersions.battery.version[1] < 0x0412)
			return;
		batteryReadRemainCap(&charge.batteryRemainCap);
		batteryReadVolt(&charge.batteryVolt);
		batteryReadLifePercent(&charge.batteryLifePercent);
		charge.flagsRead = chargeReadFlags();
		changed = ~(charge.flagsRead ^ charge.flagsLast) & (charge.flags ^ charge.flagsRead);
		if (changed != 0) {
			charge.flags ^= changed;
			chargeFactors.2 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(chargeFactors.2);
			chargeFactors.3 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(chargeFactors.3);
			chargeFactors.4 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(chargeFactors.4);
			chargeFactors.5 = (charge.flags & 0x0200) ? 1 : 0;
			sleepFactorSet19(chargeFactors.5);
			if (charge.flags & 0x0040) {
				chargeTransition(0x40);
			} else if (charge.flags & 0x0020) {
				if (!(stateGetWakeMask() & 0x40) && !(state & 0x8000) && (state & 0x0008))
					chargeTransition(0x06);
				else if (!(state & 0x8000) && (state & 0x0028))
					chargeTransition(0x32);
			}
		}
		charge.flagsLast = charge.flagsRead;
		if (state & 0x000c) {
			if (chargeControl.0 && chargeMayStart()) {
				chargeTransition(0x16);
				chargeControl.0 = 0;
			}
			if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && !(stateGetWakeMask() & 0x8000) &&
			    (!(charge.flags & 0x0004) || chargeMayStart()))
				chargeTransition(0x0f);
		}
		if (chargeBbmcChanged())
			chargeSetCurrent();
	} else if (status1.word & 0x0002) {
		chargeResetBattery();
		if (!configGet()->start.bits.batteryNoRetry && !(state & 0x8000) && (state & 0x0028))
			chargeTransition(0x32);
	}
}

/* the battery's flags: bit n set when the battery is at or below level n */
unsigned int chargeReadFlags(void)
{
	unsigned char capBelow;
	unsigned char voltBelow;
	unsigned int flags;
	unsigned char i;

	flags = 0;
	paramGetChargeLevels(chargeLevels);
	for (i = 0; i < 10; i++) {
		if ((chargeLevels[i].remainCap & 0x7fff) >= charge.batteryRemainCap)
			capBelow = 1;
		else
			capBelow = 0;
		if (chargeLevels[i].volt >= charge.batteryVolt)
			voltBelow = 1;
		else
			voltBelow = 0;
		if (chargeLevels[i].remainCap & 0x8000) {
			if (capBelow & voltBelow)
				flags |= 1 << i;
		} else if (capBelow | voltBelow) {
			flags |= 1 << i;
		}
	}
	return flags;
}

/* the battery temperature's range: the charge's settings follow it */
void chargeTempUpdate(unsigned int temp, struct ChargeTemp *t)
{
	unsigned char i;
	unsigned char cooling;
	struct ChargeTempLevel *levels;

	i = 0;
	if (stateGet() == 0x0020)
		levels = configGet()->chargeTempLevelsCharge;
	else
		levels = configGet()->chargeTempLevels;
	if (t->temp < temp)
		cooling = 0;
	else
		cooling = 1;
	for (i = 0; i < 8; i++) {
		if (temp < levels[i].limit[cooling]) {
			t->read = i;
			break;
		}
	}
	if (t->read == t->readLast) {
		if (t->read != t->level) {
			t->level = t->read;
			t->temp = temp;
		}
		if (t->setting1 != levels[t->level].setting1) {
			if (levels[t->level].setting1 == 0xff) {
				cookieCtrlCharge(0);
				if (t->level == 0)
					powerStatus.bits.cookieLock0 = 1;
				else
					powerStatus.bits.cookieLock1 = 1;
			} else {
				if (t->setting1 == 0xff) {
					powerStatus.byte[1] &= 0xcf;
					cookieCtrlCharge(1);
				}
				cookieCtrlChargingCurrent(levels[t->level].setting1);
			}
			t->setting1 = levels[t->level].setting1;
		}
		if (levels[t->level].setting2 != 0xff && t->setting2 != levels[t->level].setting2) {
			cookieCtrlReg8(levels[t->level].setting2);
			t->setting2 = levels[t->level].setting2;
		}
	}
	t->readLast = t->read;
}

/* timer 0x18: the next update, at once while the battery's flag 15 is set */
void chargeTimer(void)
{
	if (charge.flags & 0x8000)
		timerStart(0x18, 1);
	else
		timerStart(0x18, charge.acDelay);
	updEventSet(0x82);
}

void chargeUpdateSoon(void)
{
	if (!(unsigned char)sub_4D6B())
		chargeUsbMode = 0;
	updEventSet(0x82);
}

/* the Cookie's charge setting for the USB status and chargeUsbMode */
int chargeSetCurrent(void)
{
	unsigned char result;
	unsigned char setting;
	unsigned char adjust;

	result = 0xff;
	if (powerStatus.bits.usb < 4 && chargeUsbMode < 5) {
		setting = chargeCurrents[powerStatus.bits.usb][chargeUsbMode];
		if (setting == 0xff) {
			if (chargeCurrent != 0xff) {
				chargeCurrent = 0xff;
				result = powerCookieSet();
			}
		} else {
			if (cookieRegs[0].byte[0] & 0x80)
				result = powerCookieClear();
			if (deviceVersions.cookie[1] < 0x31 && setting >= 0x11)
				setting = (setting >> 1) & 0xf0;
			if (chargeBbmcVersion == 0 && powerStatus.bits.usb >= 2 && setting >= 0x11) {
				paramGetAdjust(&adjust);
				setting -= adjust;
			}
			chargeCurrent = setting;
			result = cookieCtrlSupplyCurrentUsb(setting);
		}
	}
	return result;
}

/* whether the system may start for the battery */
unsigned char chargeMayStart(void)
{
	if (configGet()->start.bits.batteryNoRetry)
		return 1;
	if (deviceVersions.battery.id == 0xfffe)
		return 1;
	if (charge.flags & 0x8000)
		return 0;
	if (charge.flags & 0x0002)
		return 0;
	if ((charge.flags & 0x0001) && !chargeFactors.0)
		return 0;
	return 1;
}

char chargeBatteryLevel9(void)
{
	return (charge.flags & 0x0200) ? 1 : 0;
}

/* the Cookie's battery setting for the battery's version */
void chargeSetBattery(void)
{
	if (deviceVersions.battery.version[2] < 0x30) {
		cookieCtrlReg7(0x20);
		return;
	}
	if (deviceVersions.battery.version[2] < 0x40) {
		cookieCtrlReg7(0x40);
		return;
	}
	if (deviceVersions.battery.version[2] < 0x50)
		cookieCtrlReg7(0x60);
}

/* whether the 3G modem's version changed since the last call: the updater has none */
unsigned char chargeBbmcChanged(void)
{
	unsigned char version;

	version = 0;
	if (chargeBbmcVersion != version) {
		chargeBbmcVersion = version;
		return 1;
	}
	return 0;
}

/* no battery */
void chargeResetBattery(void)
{
	charge.flags = charge.flagsLast = charge.flagsRead = 0x8000;
	charge.batteryBicTempC = charge.batteryTempC = charge.batteryBicTemp = charge.batteryTemp =
		charge.batteryLifePercent = charge.batteryRemainCap = charge.batteryVolt = 0;
}

/* the next update after ticks */
void chargeTimerStart(unsigned int ticks)
{
	timerStart(0x18, ticks);
}

void chargeTimerStop(void)
{
	timerStop(0x18);
}
