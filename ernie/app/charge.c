/*
 * The charge: the Cookie's status registers, read at each update, make the power status
 * (host command 0x0800); the battery's readings while one is in; and the charge's sleep
 * factors and system transitions.
 */
#pragma sfr

#include "ernie.h"
#include "battery.h"
#include "board.h"
#include "bbmc.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "kernel.h"
#include "led.h"
#include "log.h"
#include "param.h"
#include "powerreq.h"
#include "powerseq.h"
#include "sleepfactor.h"
#include "state.h"
#include "task.h"
#include "timer.h"

extern void inputChanged(void);
extern unsigned char input1Bit7Active(void);

/* chargeUpdate's tests of the int helpers: before 0x01000306 they read only the low byte */
#if FW_SINCE(0x01000306)
#define CHARGE_TEST(call)	(call)
#else
#define CHARGE_TEST(call)	((unsigned char)(call))
#endif

__sreg union CookieStatus1 cookieStatus1;
__sreg union CookieStatus2 cookieStatus2;
__sreg union PowerStatus powerStatus;
__sreg unsigned char chargeFactors;
__sreg unsigned char chargeControl;
__sreg struct ChargeTemp chargeTemp;
__sreg unsigned char chargeCurrent;
__sreg unsigned char chargeUsbMode;
#if FW_SINCE(0x00090903)
__sreg unsigned char chargeUsbHold;
#endif
__sreg unsigned char chargeVbus;
__sreg unsigned char chargeBbmcVersion;
#if FW_SINCE(0x00090903)
static __sreg unsigned char chargeReserved;
#endif
#if !FW_SINCE(0x01000107)
/* the charge LED's blink (chargeLedSetBlink before 0x01000104): it runs, and is lit; kept
   unused until 0x01000107 */
__sreg unsigned char chargeLedBlinking;
__sreg unsigned char chargeLedBlinkOn;
#endif
#if FW_SINCE(0x00090903)
__sreg unsigned char chargeCookieSetting;
#endif
#if FW_SINCE(0x01000107)
__sreg unsigned char chargeCookieHeld;
#endif

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
#if FW_SINCE(0x00090903)
	chargeUsbHold = 0;
	chargeVbus |= 3;
#else
	chargeVbus = 1;
#endif
	chargeBbmcVersion = 0;
	chargeControl.0 = 0;
	chargeControl.1 = 1;
	chargeFactors.0 = 0;
#if FW_SINCE(0x00090903)
	chargeCookieSetting = 0xc0;
#if FW_SINCE(0x01000107)
	chargeCookieHeld = 0;
#endif
	chargeAlarmInit();
#endif
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
#if FW_SINCE(0x00090903)
		cookieCtrlSupplyCurrentAc(0xc0);
		cookieCtrlCharge(1);
#else
		if (deviceVersions.cookie[1] < 0x31)
			cookieCtrlSupplyCurrentAc(0x60);
#endif
		if (deviceVersions.battery.id != 0) {
			batteryReadRemainCap(&charge.batteryRemainCap);
			batteryReadVolt(&charge.batteryVolt);
			charge.flagsLast = chargeReadFlags();
		}
		if (deviceVersions.battery.version[2] != 0)
			chargeSetBattery();
	}
#if BOARD_CHARGE_TIMER1F || (FW_SINCE(0x01000104) && !FW_SINCE(0x01000109))
	charge.timer1fPending = 0;
#endif
	handler.handler = chargeTimer;
	timerSetHandler(TIMER_RTC(0x17), &handler);
#if FW_SINCE(0x01000104)
	handler.handler = chargeCookieTimer;
	timerSetHandler(TIMER_RTC(0x1e), &handler);
	handler.handler = chargeTimer1f;
	timerSetHandler(TIMER_RTC(0x1f), &handler);
#endif
}

void chargeTransition(int transition)
{
	STATE_TRANSITION_EVENT
	STATE_TRANSITION((unsigned char)transition);
}

/* task 5's event 0x20 */
#if FW_SINCE(0x01000104)
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
	state = stateGet();
	if (status1.word != cookieStatus1.word || status2.word != cookieStatus2.word) {
		chargeSetPowerStatus();
		if ((status1.bits.ac ^ cookieStatus1.bits.ac) ||
#if FW_SINCE(0x01000109)
		    (status1.bits.charging ^ cookieStatus1.bits.charging) ||
		    (status2.bits.vbus ^ cookieStatus2.bits.vbus))
#else
		    (status1.bits.charging ^ cookieStatus1.bits.charging))
#endif
			inputChanged();
		if (cookieStatus1.bits.level)
			;
	}
	if (cookieStatus2.bits.vbusCharge) {
#if FW_SINCE(0x01000109)
		if (chargeUsbMode == 0)
			chargeUsbMode = 1;
		chargeSetCurrent();
#else
		/* before 0x01000109 only with the USB status's bit 1 */
		if (cookieStatus2.bits.usb & 2) {
			if (chargeUsbMode == 0)
				chargeUsbMode = 1;
			chargeSetCurrent();
		}
#endif
	} else {
		chargeUsbMode = 0;
		chargeUsbHold = 0;
		if (deviceVersions.cookie[1] < 0x31)
			cookieCtrlSupplyCurrentUsb(0x10);
	}
#if FW_SINCE(0x01000109)
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
#else
	/* before 0x01000109 nothing turns the AC's charge back on or clears register 3's bit 7
	   here */
	if (cookieStatus1.bits.status2 &&
	    (powerStatus.bits.cookieLock0 || powerStatus.bits.cookieLock1 || !chargeControl.1))
		cookieCtrlChargeAc(0);
	if (cookieStatus1.bits.status3 &&
	    (powerStatus.bits.cookieLock0 || powerStatus.bits.cookieLock1 ||
	     (powerStatus.bits.usb == 1 && !chargeVbus.1) || !chargeVbus.0))
		powerCookieSet();
#endif
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
		if (ticks - charge.checkTicks > configGet()->chargeTempInterval) {
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
		if (configGet()->batteryLowVolt > charge.batteryVolt)
			chargeLedSetReleased(0);
		charge.flagsRead = chargeReadFlags();
		changed = ~(charge.flagsRead ^ charge.flagsLast) & (charge.flags ^ charge.flagsRead);
		if (changed != 0) {
			dis_dsp();
			charge.flags ^= changed;
			ena_dsp();
			if (changed & 0x0200) {
				if ((charge.flags & 0x0200) && (state & 0x0008)) {
					powerOffWlan();
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
#if FW_SINCE(0x01000107)
			if (chargeFactors.5) {
				if (input1Bit7Active())
					chargeLedSetBlink(1);
				powerLedSetAlarm(1);
			} else {
				chargeLedSetBlink(0);
				powerLedSetAlarm(0);
			}
#else
			if (chargeFactors.5) {
				if (!chargeLedBlinking && input1Bit7Active())
					chargeLedSetBlink(1);
			} else {
				chargeLedSetBlink2(0);
				chargeLedSetBlink(0);
			}
#endif
			if (charge.flags & 0x0020) {
				if (!(stateGetWakeMask() & 0x40) && !(state & 0x8000) && (state & 0x0008))
					chargeTransition(0x06);
				else if (!(state & 0x8000) && (state & 0x0028))
					chargeTransition(0x37);
			}
		}
		charge.flagsLast = charge.flagsRead;
		if ((charge.flags & 0x0040) && P14.5 && !P11.4)
			chargeTransition(0x40);
		if (state & 0x000c) {
#if BOARD_CHARGE_TIMER1F
			if (((configGet()->state.bits.offWakeUnmask22 && chargeFactors.0) ||
			     chargeControl.0) && CHARGE_TEST(chargeMayStart()) && !CHARGE_TEST(chargeAlarm0On())) {
				chargeTransition(0x16);
				chargeControl.0 = 0;
				chargeTimer1fStop();
			}
#else
			if (chargeControl.0 && CHARGE_TEST(chargeMayStart()) && !CHARGE_TEST(chargeAlarm0On())) {
				chargeTransition(0x16);
				chargeControl.0 = 0;
			}
#endif
			if (!configGet()->state.bits.chargeHold && (cookieStatus2.byte[1] & 0x0c) == 0x04 &&
			    !(stateGetWakeMask() & 0x8000) && chargeUsbHold == 0 &&
			    (!(charge.flags & 0x0004) || CHARGE_TEST(chargeMayStart())) &&
			    !powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1)
				chargeTransition(0x0f);
		} else if (state & 0x0020) {
#if BOARD_CHARGE_TIMER1F
			if (configGet()->state.bits.chargeTimer1f) {
				if (!chargeFactors.0 && !charge.timer1fPending)
					chargeTimer1fStart();
				else if (chargeFactors.0 && charge.timer1fPending)
					chargeTimer1fStop();
			}
#endif
		}
		if (CHARGE_TEST(chargeBbmcChanged()))
			chargeSetCurrent();
		charge.batteryTempC = charge.batteryTemp - 0x0aac;
		charge.batteryBicTempC = charge.batteryBicTemp - 0x0aac;
		timerGetRtcTicks(&ticks);
		if (ticks - charge.tempTicks >= 0x79)
			charge.tempTicks = ticks;
	} else if (status1.bits.battery) {
		chargeResetBattery();
		if (!configGet()->start.bits.batteryNoRetry && !(state & 0x8000) && (state & 0x0028))
			chargeTransition(0x38);
		logEvent(0x0e);
	}
}
#elif FW_SINCE(0x00090903)
/* before 0x01000104 the USB's mode and the Cookie's charge lines are looked at only when a
   status register changed */
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
	state = stateGet();
	if (status1.word != cookieStatus1.word || status2.word != cookieStatus2.word) {
		chargeSetPowerStatus();
		if (cookieStatus2.bits.vbusCharge) {
			if (chargeUsbMode == 0 && !P14.5 && (cookieStatus2.bits.usb & 2)) {
				chargeUsbMode = 1;
				chargeSetCurrent();
			}
		} else if (status2.bits.vbusCharge) {
			chargeUsbMode = 0;
			chargeUsbHold = 0;
			if (deviceVersions.cookie[1] < 0x31)
				cookieCtrlSupplyCurrentUsb(0x10);
		}
		if (cookieStatus1.bits.status2 &&
		    (powerStatus.bits.cookieLock0 || powerStatus.bits.cookieLock1 || !chargeControl.1))
			cookieCtrlChargeAc(0);
		if (cookieStatus1.bits.status3 &&
		    (powerStatus.bits.cookieLock0 || powerStatus.bits.cookieLock1 ||
		     (powerStatus.bits.usb == 1 && !chargeVbus.1) || !chargeVbus.0))
			powerCookieSet();
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
	} else if (deviceVersions.battery.id == 0x0520) {
		batteryReadTemp(&charge.batteryTemp);
		batteryReadBicTemp(&charge.batteryBicTemp);
		chargeTempUpdate(charge.batteryTemp, &chargeTemp);
		timerGetRtcTicks(&ticks);
		if (ticks - charge.checkTicks > configGet()->chargeTempInterval) {
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
		charge.flagsRead = chargeReadFlags();
		changed = ~(charge.flagsRead ^ charge.flagsLast) & (charge.flags ^ charge.flagsRead);
		if (changed != 0) {
			dis_dsp();
			charge.flags ^= changed;
			ena_dsp();
			chargeFactors.2 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(chargeFactors.2);
			chargeFactors.3 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(chargeFactors.3);
			chargeFactors.4 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(chargeFactors.4);
			chargeFactors.5 = (charge.flags & 0x0200) ? 1 : 0;
			sleepFactorSet19(chargeFactors.5);
			if (chargeFactors.5) {
				if (!chargeLedBlinking && input1Bit7Active())
					chargeLedSetBlink(1);
			} else if (chargeLedBlinking) {
				chargeLedSetBlink(0);
			}
			if ((charge.flags & 0x0040) && P14.5)
				chargeTransition(0x40);
			else if (charge.flags & 0x0020) {
				if (!(stateGetWakeMask() & 0x40) && !(state & 0x8000) && (state & 0x0008))
					chargeTransition(0x06);
				else if (!(state & 0x8000) && (state & 0x0028))
					chargeTransition(0x37);
			}
		}
		charge.flagsLast = charge.flagsRead;
		if (state & 0x000c) {
			if (chargeControl.0 && CHARGE_TEST(chargeMayStart())) {
				chargeTransition(0x16);
				chargeControl.0 = 0;
			}
			if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && !(stateGetWakeMask() & 0x8000) &&
			    chargeUsbHold == 0 &&
			    (!(charge.flags & 0x0004) || CHARGE_TEST(chargeMayStart())))
				chargeTransition(0x0f);
		}
		if (CHARGE_TEST(chargeBbmcChanged()))
			chargeSetCurrent();
		charge.batteryTempC = charge.batteryTemp - 0x0aac;
		charge.batteryBicTempC = charge.batteryBicTemp - 0x0aac;
		timerGetRtcTicks(&ticks);
		if (ticks - charge.tempTicks >= 0x79)
			charge.tempTicks = ticks;
	} else if (status1.bits.battery) {
		chargeResetBattery();
		if (!configGet()->start.bits.batteryNoRetry && !(state & 0x8000) && (state & 0x0028))
			chargeTransition(0x38);
		logEvent(0x0e);
	}
}
#else
/* 0x00090804's: the power status made here, no Cookie charge lines, and the temperature's
   ranges followed for any battery */
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
	reg = COOKIE_REG(1);
	cookieRead(&reg, cookieStatus1.byte);
	reg = COOKIE_REG(2);
	cookieRead(&reg, cookieStatus2.byte);
	state = stateGet();
	ac = chargeFactors.0;
	if (status1.word != cookieStatus1.word || status2.word != cookieStatus2.word) {
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
		if (chargeFactors.0 != ac) {
			if (chargeFactors.0) {
				charge.acDelay = 10;
				if (!(stateGetWakeMask() & 0x00400000))
					chargeControl.0 = 1;
			} else {
				chargeControl.0 = 0;
				charge.acDelay = 0x3c;
			}
			timerStart(TIMER_RTC(0x17), charge.acDelay);
		}
		if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && (cookieStatus1.byte[1] & 0xc0) == 0x80)
			chargeFactors.1 = 1;
		else
			chargeFactors.1 = 0;
		sleepFactorSetStatus4(chargeFactors.1);
		ena_dsp();
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
			dis_dsp();
			charge.flags ^= changed;
			ena_dsp();
			chargeFactors.2 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(chargeFactors.2);
			chargeFactors.3 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(chargeFactors.3);
			chargeFactors.4 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(chargeFactors.4);
			chargeFactors.5 = (charge.flags & 0x0200) ? 1 : 0;
			sleepFactorSet19(chargeFactors.5);
			if (chargeFactors.5) {
				if (!chargeLedBlinking && input1Bit7Active())
					chargeLedSetBlink(1);
			} else if (chargeLedBlinking) {
				chargeLedSetBlink(0);
			}
			if (charge.flags & 0x0040)
				chargeTransition(0x40);
			else if (charge.flags & 0x0020) {
				if (!(stateGetWakeMask() & 0x40) && !(state & 0x8000) && (state & 0x0008))
					chargeTransition(0x06);
				else if (!(state & 0x8000) && (state & 0x0028))
					chargeTransition(0x32);
			}
		}
		charge.flagsLast = charge.flagsRead;
		if (state & 0x000c) {
			if (chargeControl.0 && CHARGE_TEST(chargeMayStart())) {
				chargeTransition(0x16);
				chargeControl.0 = 0;
			}
			if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && !(stateGetWakeMask() & 0x8000) &&
			    (!(charge.flags & 0x0004) || CHARGE_TEST(chargeMayStart())))
				chargeTransition(0x0f);
		}
		if (CHARGE_TEST(chargeBbmcChanged()))
			chargeSetCurrent();
		charge.batteryTempC = charge.batteryTemp - 0x0aac;
		charge.batteryBicTempC = charge.batteryBicTemp - 0x0aac;
		timerGetRtcTicks(&charge.checkTicks);
		if (charge.checkTicks - charge.tempTicks >= 0x79)
			charge.tempTicks = charge.checkTicks;
	} else if (status1.word & 0x0002) {
		chargeResetBattery();
		if (!configGet()->start.bits.batteryNoRetry && !(state & 0x8000) && (state & 0x0028))
			chargeTransition(0x32);
		logEvent(0x0e);
	}
}
#endif

#if FW_SINCE(0x00090903)
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
#if !FW_SINCE(0x01000306)
			charge.acDelay = 10;
#endif
			if (!(stateGetWakeMask() & 0x00400000))
				chargeControl.0 = 1;
		} else {
			chargeControl.0 = 0;
#if !FW_SINCE(0x01000104)
			if (stateGet() & 0x000c)
				charge.acDelay = 60;
#endif
		}
#if FW_SINCE(0x01000306)
		chargeSetAcDelay();
#endif
		timerStart(TIMER_RTC(0x17), charge.acDelay);
	}
	if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && (cookieStatus1.byte[1] & 0xc0) == 0x80)
		chargeFactors.1 = 1;
	else
		chargeFactors.1 = 0;
	sleepFactorSetStatus4(chargeFactors.1);
	ena_dsp();
}
#endif

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
				if (t->level == 0) {
					powerStatus.bits.cookieLock0 = 1;
					logEvent(9);
				} else {
					powerStatus.bits.cookieLock1 = 1;
					logEvent(0x0a);
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

#if FW_SINCE(0x00090903)
/* the battery temperature's alarms: each one's handler runs when its temperature has stayed
   past its limits, or back, for the limit's time */
void chargeAlarmUpdate(void)
{
	unsigned int temp;
	unsigned int hold;
	unsigned int ticks;
	unsigned char i;
#if FW_SINCE(0x01000104)
	struct ChargeAlarmLimit *limits;

	if (configGet()->board.bits.bbmcDetect)
		limits = configGet()->chargeAlarmLimits3G;
	else
		limits = configGet()->chargeAlarmLimits;
#else
	/* before 0x01000104 no 3G modem's limits, and the config is looked up each time */
#define limits	(configGet()->chargeAlarmLimits)
#endif
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
#undef limits

/* alarm 0's: sleep factor 23, and timer 0x1d */
static void chargeAlarm0(void)
{
	if (charge.alarms[0].flags.bits.on) {
		charge.alarmsOn.bits.alarm0 = 1;
		timerStart(TIMER_RTC(0x1d), configGet()->chargeAlarm0Delay);
	} else {
		charge.alarmsOn.bits.alarm0 = 0;
	}
	chargeFactors.7 = charge.alarms[0].flags.bits.on;
	sleepFactorSet23(chargeFactors.7);
}

/* timer 0x1d */
static void chargeAlarm0Timer(void)
{
	if (stateGet() == 0x0020)
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
		chargeCookieSetting = configGet()->chargeAlarm2Setting;
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
	timerSetHandler(TIMER_RTC(0x1d), &handler);
}
#endif

/* timer 0x17: the next update, at once while the battery's flag 15 is set */
void chargeTimer(void)
{
	if (charge.flags & 0x8000)
		timerStart(TIMER_RTC(0x17), 1);
	else
		timerStart(TIMER_RTC(0x17), charge.acDelay);
	task5SetFlag(4, 0x0020);
}

#if FW_SINCE(0x01000104)
/* timer 0x1e (powerCookieHold): task 5 runs powerCookieRelease */
void chargeCookieTimer(void)
{
	task5SetFlag(4, 0x0400);
}

/* timer 0x1f */
void chargeTimer1f(void)
{
	if (!(stateGet() & 0x0004))
		chargeTransition(0x3a);
	charge.timer1fPending = 0;
}
#endif

void chargeUpdateSoon(void)
{
#if FW_SINCE(0x00090903)
	dly_tsk(5);
#else
	if (!input1Bit7Active())
		chargeUsbMode = 0;
#endif
	task5SetFlag(4, 0x0020);
	task5SetFlag(4, 0x1000);
}

#if FW_SINCE(0x01000104)
/* the Cookie's charge setting for the USB status and chargeUsbMode */
int chargeSetCurrent(void)
{
	unsigned char result;
	unsigned char setting;
	unsigned char adjust;

	result = 0xff;
	if (powerStatus.bits.usb < 4 && chargeUsbMode < 6) {
		setting = configGet()->chargeCurrents[powerStatus.bits.usb][chargeUsbMode];
		if (setting == 0xff || (powerStatus.bits.usb == 1 && !chargeVbus.1) || !chargeVbus.0) {
			chargeCurrent = 0xff;
			result = powerCookieSet();
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
#elif FW_SINCE(0x00090903)
/* before 0x01000104 the settings are the program's (the config's chargeCurrents later), and
   the Cookie's charge is set again only when it was not */
static const unsigned char chargeCurrents[4][6] = {
	{ 0x10, 0x10, 0x10, 0x10, 0xff, 0x10 },
	{ 0x10, 0x10, 0x10, 0x50, 0xff, 0x10 },
	{ 0x10, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0 },
	{ 0x10, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0 },
};

int chargeSetCurrent(void)
{
	unsigned char result;
	unsigned char setting;
	unsigned char adjust;

	result = 0xff;
	if (powerStatus.bits.usb < 4 && chargeUsbMode < 6) {
		setting = chargeCurrents[powerStatus.bits.usb][chargeUsbMode];
		if (setting == 0xff || (powerStatus.bits.usb == 1 && !chargeVbus.1) || !chargeVbus.0) {
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
#else
/* 0x00090804's (the updater's): five USB modes, and no VBUS bits */
static const unsigned char __far chargeCurrents[4][5] = {
	{ 0x10, 0x10, 0x10, 0x10, 0xff },
	{ 0x10, 0x10, 0x10, 0x50, 0xff },
	{ 0x10, 0xc0, 0xc0, 0xc0, 0xc0 },
	{ 0x10, 0xc0, 0xc0, 0xc0, 0xc0 },
};

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
#endif

#if FW_SINCE(0x01000104)
/* whether the system may start for the battery */
int chargeMayStart(void)
{
	unsigned int state;

	state = stateGet();
	if (configGet()->start.bits.batteryNoRetry)
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
	/* Sony's returns nothing here: BC still holds configGet's pointer */
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
#else
/* before 0x01000104 alarm 0 is looked at here (0x00090804 has none), and the battery's
   levels whatever the state */
int chargeMayStart(void)
{
#if FW_SINCE(0x00090903)
	if (charge.alarmsOn.byte & 0x01)
		return 0;
#endif
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
#endif

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
#if FW_SINCE(0x01000104)
	if (deviceVersions.battery.version[2] < 0x50) {
		cookieCtrlReg7(configGet()->batterySetting4x);
		return;
	}
	cookieCtrlReg7(configGet()->batterySetting5x);
#else
	if (deviceVersions.battery.version[2] < 0x50)
		cookieCtrlReg7(0x60);
#endif
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
	timerStart(TIMER_RTC(0x17), ticks);
}

void chargeTimerStop(void)
{
	clr_flg(4, ~0x0020);
	timerStop(TIMER_RTC(0x17));
}

/* for the touch frames' power bit and powerreq (before 0x01000104 neither, on boards without BOARD_TOUCH_CHARGE) */
#if FW_SINCE(0x01000104) || BOARD_TOUCH_CHARGE
unsigned char chargeGetAc(void)
{
	return powerStatus.bits.ac;
}

unsigned char chargeGetCharging(void)
{
	return powerStatus.bits.charging;
}
#endif

#if FW_SINCE(0x01000107)
/* randomGenerate's (before 0x01000107 the generator does not read the battery) */
void chargeReadTempVolt(unsigned int *value)
{
	*value = charge.batteryTemp + charge.batteryVolt;
}
#endif

#if FW_SINCE(0x01000109)
/* timer 0x1f (chargeTimer1f), on PCH-1000 only: the others have no caller (before 0x01000109
   neither function exists, nor chargeUsbIs1 and chargeUsbIs2) */
void chargeTimer1fStart(void)
{
#if BOARD_CHARGE_TIMER1F
	timerStart(TIMER_RTC(0x1f), configGet()->chargeTimer1fDelay + 120);
	charge.timer1fPending = 1;
#endif
}

void chargeTimer1fStop(void)
{
#if BOARD_CHARGE_TIMER1F
	charge.timer1fPending = 0;
	timerStop(TIMER_RTC(0x1f));
#endif
}
#endif

#if FW_SINCE(0x01000306)
/* the update's period while the power status's AC is set, or in system states 4, 8 and 0x20 */
void chargeSetAcDelay(void)
{
	if (chargeFactors.0) {
		charge.acDelay = 10;
		return;
	}
	switch (stateGet() & 0x7fff) {
	case 0x0020:
		charge.acDelay = 10;
		break;
	case 0x0004:
	case 0x0008:
#if BOARD_CHARGE_TIMER1F
		charge.acDelay = 60;
#else
		charge.acDelay = 10;
#endif
		break;
	}
}
#endif

#if FW_SINCE(0x01000109)
int chargeUsbIs1(void)
{
	return powerStatus.bits.usb == 1;
}

int chargeUsbIs2(void)
{
	return cookieStatus2.bits.vbus && powerStatus.bits.usb == 2;
}
#endif
