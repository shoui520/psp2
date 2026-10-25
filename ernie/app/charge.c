/*
 * The charge: the Cookie's status registers, read at each update, make the power status
 * (host command 0x0800); the battery's readings while one is in; and the charge's sleep
 * factors and system transitions.
 */
#pragma sfr

#include "ernie.h"
#include "battery.h"
#include "bbmc.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "kernel.h"
#include "powerreq.h"
#include "powerseq.h"
#include "sleepfactor.h"
#include "timer.h"

extern unsigned char sub_2AB49(int transition);
extern unsigned int sub_2AB46(void);
extern unsigned long sub_2AA79(void);
extern void sub_1276A(void);
extern unsigned char sub_126C9(void);
extern void sub_2DAE1(int);
extern void sub_2DFCE(unsigned int);
extern void sub_2E03A(unsigned int);
extern void sub_2E082(unsigned int);
extern void sub_2C24D(unsigned char *adjust);
extern void sub_2C315(ID flgid, FLGPTN setptn);
extern void sub_2C1D7(struct ChargeLevel *levels);

__sreg union CookieStatus1 cookieStatus1;
__sreg union CookieStatus2 cookieStatus2;
__sreg union PowerStatus powerStatus;
__sreg unsigned char chargeFactors;
__sreg unsigned char chargeControl;
__sreg struct ChargeTemp chargeTemp;
__sreg unsigned char chargeCurrent;
__sreg unsigned char chargeUsbMode;
__sreg unsigned char chargeUsbHold;
__sreg unsigned char chargeVbus;
__sreg unsigned char chargeBbmcVersion;
static __sreg unsigned char chargeReserved;
__sreg unsigned char chargeCookieSetting;
__sreg unsigned char chargeCookieHeld;

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
	chargeUsbHold = 0;
	chargeVbus |= 3;
	chargeBbmcVersion = 0;
	chargeControl.0 = 0;
	chargeControl.1 = 1;
	chargeFactors.0 = 0;
	chargeCookieSetting = 0xc0;
	chargeCookieHeld = 0;
	chargeAlarmInit();
	if (deviceVersions.cookie[0] != 0x80) {
		charge.batteryVolt = sub_2C658()->batteryVolt;
		charge.batteryRemainCap = sub_2C658()->batteryRemainCap;
		charge.batteryLifePercent = sub_2C658()->batteryLifePercent;
		charge.batteryTemp = sub_2C658()->batteryTemp;
		charge.batteryBicTemp = sub_2C658()->batteryBicTemp;
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
		cookieCtrlSupplyCurrentAc(0xc0);
		cookieCtrlCharge(1);
		if (deviceVersions.battery.id != 0) {
			batteryReadRemainCap(&charge.batteryRemainCap);
			batteryReadVolt(&charge.batteryVolt);
			charge.flagsLast = chargeReadFlags();
		}
		if (deviceVersions.battery.version[2] != 0)
			chargeSetBattery();
	}
	handler.handler = chargeTimer;
	timerSetHandler(0x17, &handler);
	handler.handler = chargeCookieTimer;
	timerSetHandler(0x1e, &handler);
	handler.handler = chargeTimer1f;
	timerSetHandler(0x1f, &handler);
}

void chargeTransition(int transition)
{
	sub_2AB49((unsigned char)transition);
}

/* task 5's event 0x20 */
void chargeUpdate(void)
{
	union CookieStatus1 status1;
	union CookieStatus2 status2;
	unsigned int changed;
	unsigned int state;
	unsigned int ticks;

	if (deviceVersions.cookie[0] != 0x80)
		return;
	status1 = cookieStatus1;
	status2 = cookieStatus2;
	cookieReadStatus1(cookieStatus1.byte);
	cookieReadStatus2(cookieStatus2.byte);
	cookieReadReg3();
	state = sub_2AB46();
	if (status1.word != cookieStatus1.word || status2.word != cookieStatus2.word) {
		chargeSetPowerStatus();
		if ((status1.bits.ac ^ cookieStatus1.bits.ac) ||
		    (status1.bits.charging ^ cookieStatus1.bits.charging) ||
		    (status2.bits.vbus ^ cookieStatus2.bits.vbus))
			sub_1276A();
		if (cookieStatus1.bits.level)
			;
	}
	if (cookieStatus2.bits.vbusCharge) {
		if (chargeUsbMode == 0)
			chargeUsbMode = 1;
		chargeSetCurrent();
	} else {
		chargeUsbMode = 0;
		chargeUsbHold = 0;
		if (deviceVersions.cookie[1] < 0x31)
			cookieCtrlSupplyCurrentUsb(0x10);
	}
	if (powerStatus.bits.cookieLock0 || powerStatus.bits.cookieLock1 || !chargeControl.1) {
		if (cookieStatus1.bits.status2)
			cookieCtrlChargeAc(0);
	} else if (cookieRegs[0].byte[0] & 0x40) {
		cookieCtrlChargeAc(1);
	}
	if (powerStatus.bits.cookieLock0 || powerStatus.bits.cookieLock1 ||
	    (powerStatus.bits.usb == 1 && !chargeVbus.1) || !chargeVbus.0) {
		if (cookieStatus1.bits.status3)
			powerCookieSet();
	} else if (chargeCurrent != 0xff && (cookieRegs[0].byte[0] & 0x80)) {
		powerCookieClear();
	}
	if (deviceVersions.battery.id == 0) {
		if (batteryReadInfo(&deviceVersions.battery.id) == 0) {
			chargeSetBattery();
		} else {
			deviceVersions.battery.id = deviceVersions.battery.version[0] =
				deviceVersions.battery.version[1] = deviceVersions.battery.version[2] = 0;
			return;
		}
	} else if (deviceVersions.battery.id == 0x0520) {
		batteryReadTemp(&charge.batteryTemp);
		batteryReadBicTemp(&charge.batteryBicTemp);
		chargeTempUpdate(charge.batteryTemp, &chargeTemp);
		timerGetRtcTicks(&ticks);
		if (ticks - charge.checkTicks > sub_2C658()->chargeTempInterval) {
			charge.checkTicks = ticks;
			chargeAlarmUpdate();
		}
	}
	if (powerStatus.bits.battery) {
		if (!status1.bits.battery && deviceVersions.battery.version[1] >= 0x0408 &&
		    deviceVersions.battery.version[1] < 0x0412)
			return;
		batteryReadRemainCap(&charge.batteryRemainCap);
		batteryReadVolt(&charge.batteryVolt);
		batteryReadLifePercent(&charge.batteryLifePercent);
		if (sub_2C658()->batteryLowVolt > charge.batteryVolt)
			sub_2E03A(0);
		charge.flagsRead = chargeReadFlags();
		changed = ~(charge.flagsRead ^ charge.flagsLast) & (charge.flags ^ charge.flagsRead);
		if (changed != 0) {
			dis_dsp();
			charge.flags ^= changed;
			ena_dsp();
			if (changed & 0x0200) {
				if ((charge.flags & 0x0200) && (state & 0x0008)) {
					powerWirelessOff();
					powerBbmcCycle();
				}
				chargeFactors.5 = (charge.flags & 0x0200) ? 1 : 0;
				sleepFactorSet19(chargeFactors.5);
			}
			chargeFactors.2 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(chargeFactors.2);
			chargeFactors.3 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(chargeFactors.3);
			chargeFactors.4 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(chargeFactors.4);
			if (chargeFactors.5) {
				if (sub_126C9())
					sub_2DFCE(1);
				sub_2E082(1);
			} else {
				sub_2DFCE(0);
				sub_2E082(0);
			}
			if (charge.flags & 0x0020) {
				if (!(sub_2AA79() & 0x40) && !(state & 0x8000) && (state & 0x0008))
					chargeTransition(0x06);
				else if (!(state & 0x8000) && (state & 0x0028))
					chargeTransition(0x37);
			}
		}
		charge.flagsLast = charge.flagsRead;
		if ((charge.flags & 0x0040) && P14.5 && !P11.4)
			chargeTransition(0x40);
		if (state & 0x000c) {
			if (chargeControl.0 && chargeMayStart() && !chargeAlarm0On()) {
				chargeTransition(0x16);
				chargeControl.0 = 0;
			}
			if (!sub_2C658()->chargeHold && (cookieStatus2.byte[1] & 0x0c) == 0x04 &&
			    !(sub_2AA79() & 0x8000) && chargeUsbHold == 0 &&
			    (!(charge.flags & 0x0004) || chargeMayStart()) &&
			    !powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1)
				chargeTransition(0x0f);
		} else if (state & 0x0020) {
		}
		if (chargeBbmcChanged())
			chargeSetCurrent();
		charge.batteryTempC = charge.batteryTemp - 0x0aac;
		charge.batteryBicTempC = charge.batteryBicTemp - 0x0aac;
		timerGetRtcTicks(&ticks);
		if (ticks - charge.tempTicks >= 0x79)
			charge.tempTicks = ticks;
	} else if (status1.bits.battery) {
		chargeResetBattery();
		if (!sub_2C658()->batteryNoRetry && !(state & 0x8000) && (state & 0x0028))
			chargeTransition(0x38);
		sub_2DAE1(0x0e);
	}
}

/* the power status and the charge's sleep factors from the Cookie's status registers */
void chargeSetPowerStatus(void)
{
	unsigned char ac;

	ac = chargeFactors.0;
	dis_dsp();
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
	if (ac != chargeFactors.0) {
		if (chargeFactors.0) {
			if (!(sub_2AA79() & 0x00400000))
				chargeControl.0 = 1;
		} else {
			chargeControl.0 = 0;
		}
		chargeSetAcDelay();
		timerStart(0x17, charge.acDelay);
	}
	if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && (cookieStatus1.byte[1] & 0xc0) == 0x80)
		chargeFactors.1 = 1;
	else
		chargeFactors.1 = 0;
	sleepFactorSetStatus4(chargeFactors.1);
	ena_dsp();
}

/* the battery's flags: bit n set when the battery is at or below level n */
unsigned int chargeReadFlags(void)
{
	unsigned char capBelow;
	unsigned char voltBelow;
	unsigned int flags;
	unsigned char i;

	flags = 0;
	sub_2C1D7(chargeLevels);
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
	if (sub_2AB46() == 0x0020)
		levels = sub_2C658()->chargeTempLevelsCharge;
	else
		levels = sub_2C658()->chargeTempLevels;
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
				if (t->level == 0) {
					powerStatus.bits.cookieLock0 = 1;
					sub_2DAE1(9);
				} else {
					powerStatus.bits.cookieLock1 = 1;
					sub_2DAE1(0x0a);
				}
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

/* the battery temperature's alarms: each one's handler runs when its temperature has stayed
   past its limits, or back, for the limit's time */
void chargeAlarmUpdate(void)
{
	unsigned int temp;
	unsigned int hold;
	unsigned int ticks;
	unsigned char i;
	struct ChargeAlarmLimit *limits;

	if (sub_2C658()->bbmcDetect)
		limits = sub_2C658()->chargeAlarmLimits3G;
	else
		limits = sub_2C658()->chargeAlarmLimits;
	for (i = 0; i < 4; i++) {
		if (limits[i].bicTemp == 0)
			temp = charge.batteryTemp;
		else
			temp = charge.batteryBicTemp;
		if (temp >= limits[i].high)
			charge.alarms[i].flags.bits.over = 1;
		else if (limits[i].low >= temp)
			charge.alarms[i].flags.bits.over = 0;
		else
			charge.alarms[i].flags.bits.over = charge.alarms[i].flags.bits.on;
		if (charge.alarms[i].flags.bits.over != charge.alarms[i].flags.bits.on) {
			timerGetRtcTicks(&ticks);
			if (charge.alarms[i].flags.bits.over != charge.alarms[i].flags.bits.overLast)
				charge.alarms[i].since = ticks;
			if (charge.alarms[i].flags.bits.over)
				hold = limits[i].holdOn;
			else
				hold = limits[i].holdOff;
			if (ticks - charge.alarms[i].since >= hold) {
				charge.alarms[i].flags.bits.on = charge.alarms[i].flags.bits.over;
				charge.alarms[i].handler();
			}
		}
		charge.alarms[i].flags.bits.overLast = charge.alarms[i].flags.bits.over;
	}
}

/* alarm 0's: sleep factor 23, and timer 0x1d */
static void chargeAlarm0(void)
{
	if (charge.alarms[0].flags.bits.on) {
		charge.alarmsOn.bits.alarm0 = 1;
		timerStart(0x1d, sub_2C658()->chargeAlarm0Delay);
	} else {
		charge.alarmsOn.bits.alarm0 = 0;
	}
	chargeFactors.7 = charge.alarms[0].flags.bits.on;
	sleepFactorSet23(chargeFactors.7);
}

/* timer 0x1d */
static void chargeAlarm0Timer(void)
{
	if (sub_2AB46() == 0x0020)
		chargeTransition(0x39);
}

/* alarm 1's: sleep factor 22 */
static void chargeAlarm1(void)
{
	if (charge.alarms[1].flags.bits.on)
		charge.alarmsOn.bits.alarm1 = 1;
	else
		charge.alarmsOn.bits.alarm1 = 0;
	chargeFactors.6 = charge.alarms[1].flags.bits.on;
	sleepFactorSet22(chargeFactors.6);
}

/* alarm 2's: the Cookie's setting cookieCtrlSupplyCurrentAc */
static void chargeAlarm2(void)
{
	if (charge.alarms[2].flags.bits.on) {
		charge.alarmsOn.bits.alarm2 = 1;
		chargeCookieSetting = sub_2C658()->chargeAlarm2Setting;
	} else {
		charge.alarmsOn.bits.alarm2 = 0;
		chargeCookieSetting = 0xc0;
	}
	chargeSetCurrent();
	cookieCtrlSupplyCurrentAc(chargeCookieSetting);
}

static void chargeAlarm3(void)
{
}

void chargeAlarmInit(void)
{
	struct TimerHandler handler;

	charge.checkTicks = 0;
	charge.alarmsOn.byte = 0;
	charge.alarms[0].flags.byte = 0;
	charge.alarms[0].since = 0;
	charge.alarms[0].handler = chargeAlarm0;
	charge.alarms[1].flags.byte = 0;
	charge.alarms[1].since = 0;
	charge.alarms[1].handler = chargeAlarm1;
	charge.alarms[2].flags.byte = 0;
	charge.alarms[2].since = 0;
	charge.alarms[2].handler = chargeAlarm2;
	charge.alarms[3].flags.byte = 0;
	charge.alarms[3].since = 0;
	charge.alarms[3].handler = chargeAlarm3;
	handler.handler = chargeAlarm0Timer;
	timerSetHandler(0x1d, &handler);
}

/* timer 0x17: the next update, at once while the battery's flag 15 is set */
void chargeTimer(void)
{
	if (charge.flags & 0x8000)
		timerStart(0x17, 1);
	else
		timerStart(0x17, charge.acDelay);
	sub_2C315(4, 0x0020);
}

/* timer 0x1e (powerCookieHold): task 5 runs powerCookieRelease */
void chargeCookieTimer(void)
{
	sub_2C315(4, 0x0400);
}

/* timer 0x1f */
void chargeTimer1f(void)
{
	if (!(sub_2AB46() & 0x0004))
		chargeTransition(0x3a);
	charge.timer1fPending = 0;
}

void chargeUpdateSoon(void)
{
	dly_tsk(5);
	sub_2C315(4, 0x0020);
	sub_2C315(4, 0x1000);
}

/* the Cookie's charge setting for the USB status and chargeUsbMode */
int chargeSetCurrent(void)
{
	unsigned char result;
	unsigned char setting;
	unsigned char adjust;

	result = 0xff;
	if (powerStatus.bits.usb < 4 && chargeUsbMode < 6) {
		setting = sub_2C658()->chargeCurrents[powerStatus.bits.usb][chargeUsbMode];
		if (setting == 0xff || (powerStatus.bits.usb == 1 && !chargeVbus.1) || !chargeVbus.0) {
			chargeCurrent = 0xff;
			result = powerCookieSet();
		} else {
			if (cookieRegs[0].byte[0] & 0x80)
				result = powerCookieClear();
			if (deviceVersions.cookie[1] < 0x31 && setting >= 0x11)
				setting = (setting >> 1) & 0xf0;
			if (chargeBbmcVersion == 0 && powerStatus.bits.usb >= 2 && setting >= 0x11) {
				sub_2C24D(&adjust);
				setting -= adjust;
			}
			chargeCurrent = setting;
			result = cookieCtrlSupplyCurrentUsb(setting);
		}
	}
	return result;
}

/* whether the system may start for the battery */
int chargeMayStart(void)
{
	unsigned int state;

	state = sub_2AB46();
	if (sub_2C658()->batteryNoRetry)
		return 1;
	if (deviceVersions.battery.id == 0xfffe)
		return 1;
	if (charge.flags & 0x8000)
		return 0;
	if (state & 0x0004) {
		if (chargeBatteryLevel1())
			return 0;
		if (chargeBatteryLevel0() && !chargeFactors.0)
			return 0;
		return 1;
	}
	if (state & 0x0008) {
		if (chargeBatteryLevel9())
			return 0;
		return 1;
	}
	/* Sony's returns nothing here: BC still holds sub_2C658's pointer */
}

int chargeAlarm0On(void)
{
	if (charge.alarmsOn.byte & 0x01)
		return 1;
	return 0;
}

char chargeBatteryLevel9(void)
{
	return (charge.flags & 0x0200) ? 1 : 0;
}

char chargeBatteryLevel0(void)
{
	return (charge.flags & 0x0001) ? 1 : 0;
}

char chargeBatteryLevel1(void)
{
	return (charge.flags & 0x0002) ? 1 : 0;
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
	if (deviceVersions.battery.version[2] < 0x50) {
		cookieCtrlReg7(sub_2C658()->batterySetting4x);
		return;
	}
	cookieCtrlReg7(sub_2C658()->batterySetting5x);
}

/* whether the 3G modem's version changed since the last call */
int chargeBbmcChanged(void)
{
	unsigned char version;

	version = bbmcGetVersion();
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
	clr_flg(4, ~0x0020);
	timerStart(0x17, ticks);
}

void chargeTimerStop(void)
{
	clr_flg(4, ~0x0020);
	timerStop(0x17);
}

unsigned char chargeGetAc(void)
{
	return powerStatus.bits.ac;
}

unsigned char chargeGetCharging(void)
{
	return powerStatus.bits.charging;
}

void chargeReadTempVolt(unsigned int *value)
{
	*value = charge.batteryTemp + charge.batteryVolt;
}

/* no caller */
void chargeUnused0(void)
{
}

void chargeUnused1(void)
{
}

/* the update's period while the power status's AC is set, or in system states 4, 8 and 0x20 */
void chargeSetAcDelay(void)
{
	if (chargeFactors.0) {
		charge.acDelay = 10;
		return;
	}
	switch (sub_2AB46() & 0x7fff) {
	case 0x0020:
		charge.acDelay = 10;
		break;
	case 0x0004:
	case 0x0008:
		charge.acDelay = 10;
		break;
	}
}

int chargeUsbIs1(void)
{
	return powerStatus.bits.usb == 1;
}

int chargeUsbIs2(void)
{
	return cookieStatus2.bits.vbus && powerStatus.bits.usb == 2;
}
