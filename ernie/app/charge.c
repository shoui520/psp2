/*
 * The charge: the Cookie's status registers, read at each update, make the power status
 * (host command 0x0800); the battery's readings while one is in; and the charge's sleep
 * factors and system transitions.
 */
#pragma sfr

#include "ernie.h"
#include "analog.h"
#include "battery.h"
#include "board.h"
#include "bbmc.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "eventlog.h"
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

/* chargeUpdate's tests of the int helpers: before 0x01000306 and since 0x0100060B they read only the low byte */
#if FW_SINCE(0x01000306) && !FW_SINCE(0x0100060B)
#define CHARGE_TEST(call)	(call)
#else
#define CHARGE_TEST(call)	((unsigned char)(call))
#endif

/* the timer of chargeUpdateSoon, below the RTC's */
#if BOARD_DOL1001
#define CHARGE_SOON_TIMER	0x14
#elif BOARD_USS1001 && SOURCE_0104
#define CHARGE_SOON_TIMER	0x0f	/* the slim's 0x0105 */
#elif SOURCE_0103
#define CHARGE_SOON_TIMER	0x13
#else
#define CHARGE_SOON_TIMER	0x16
#endif

__sreg union CookieStatus1 cookieStatus1;
__sreg union CookieStatus2 cookieStatus2;
#if SOURCE_0103
__sreg union CookieStatus1 chargeStatus1Last;
__sreg union CookieStatus2 chargeStatus2Last;
#endif
__sreg union PowerStatus powerStatus;
#if SOURCE_0103
__sreg union ChargeFactors chargeFactors;
#else
__sreg unsigned char chargeFactors;
#endif
#if CHARGE_0104
__sreg unsigned char chargeCommand0903[2];
#endif
#if FW_SINCE(0x00090009)
__sreg unsigned char chargeControl;
#endif
/* bit 0, the transition 0x16 due at the AC: 0x00090206 keeps it in the whole byte */
#if FW_SINCE(0x00090804)
#define CHARGE_AC_DUE		chargeControl.0
#else
#define CHARGE_AC_DUE		chargeControl
#endif
__sreg struct ChargeTemp chargeTemp;
__sreg unsigned char chargeCurrent;
#if SOURCE_0103
__sreg volatile unsigned char chargeUsbMode;
#else
__sreg unsigned char chargeUsbMode;
#endif
#if FW_SINCE(0x00090903)
__sreg unsigned char chargeUsbHold;
#endif
__sreg unsigned char chargeVbus;
__sreg unsigned char chargeBbmcVersion;
#if !FW_SINCE(0x00090009)
__sreg unsigned char chargeControl;	/* 0x00080504's place */
#endif
#if FW_SINCE(0x00090903)
static __sreg unsigned char chargeReserved;
#endif
#if FW_SINCE(0x00090206) && !FW_SINCE(0x01000107)
/* the charge LED's blink (chargeLedSetBlink before 0x01000104): it runs, and is lit; kept
   unused until 0x01000107 (0x00090009 has no blink) */
__sreg unsigned char chargeLedBlinking;
__sreg unsigned char chargeLedBlinkOn;
#endif
#if SOURCE_0103
__sreg unsigned int chargeBoostTicks;
__sreg union ChargeBoost chargeBoost;
#endif
#if FW_SINCE(0x00090903)
__sreg unsigned char chargeCookieSetting;
#endif
#if SOURCE_0103
__sreg unsigned char chargeTempSetting;
#endif
#if FW_SINCE(0x01000107)
__sreg unsigned char chargeCookieHeld;
#endif
#if SOURCE_0103
__sreg unsigned char chargeManualMode;
#if CHARGE_0104
__sreg unsigned char chargeSerialMode;
#endif
__sreg unsigned char chargeUsbState;
__sreg unsigned char chargeUsbWait;
__sreg unsigned char chargeUsbTicks;
#endif

#if FW_SINCE(0x00090206)
#define CHARGE_LEVELS	10
#else
#define CHARGE_LEVELS	9	/* 0x00090009's nine levels */
#endif
static struct ChargeLevel chargeLevels[CHARGE_LEVELS];
#if !FW_SINCE(0x00090009)
/* 0x00080504's temperature levels are the program's (the config's later): limit[2], setting1,
   setting2 */
static const struct ChargeTempLevel chargeTempLevels[6] = {
	{ { 0x0ade, 0x0aca }, 0xff, 0xff },
	{ { 0x0b42, 0x0b2e }, 0xc0, 0x50 },
	{ { 0x0c50, 0x0c3c }, 0xc0, 0x10 },
	{ { 0x0c82, 0x0c6e }, 0xc0, 0x50 },
	{ { 0x0ce6, 0x0cd2 }, 0xc0, 0x80 },
	{ { 0xffff, 0xffff }, 0xff, 0xff },
};

/* and while charging (state 0x20) */
static const struct ChargeTempLevel chargeTempLevelsCharge[6] = {
	{ { 0x0ade, 0x0aca }, 0xff, 0xff },
	{ { 0x0b42, 0x0b2e }, 0xc0, 0x50 },
	{ { 0x0c50, 0x0c3c }, 0xc0, 0x50 },
	{ { 0x0c82, 0x0c6e }, 0xc0, 0x50 },
	{ { 0x0ce6, 0x0cd2 }, 0xc0, 0x80 },
	{ { 0xffff, 0xffff }, 0xff, 0xff },
};
#define CHARGE_TEMP_LEVELS	6
#else
#define CHARGE_TEMP_LEVELS	8
#endif

struct Charge charge;
#if !FW_SINCE(0x00090009)
/* 0x00080504 keeps the two tick counts out of the structure (charge.tempTicks and checkTicks
   later) */
static unsigned int chargeTempTicks;
static unsigned int chargeCheckTicks;
#endif

void chargeInit(void)
{
	struct TimerHandler handler;

#if BOARD_DOL1001
	/* DOL-1001's: the slim's, with no battery to ask: the settings' readings while the Cookie
	   has not answered 0x80, and the AC in */
	cookieStatus1.word = 0;
	cookieStatus2.word = 0;
	chargeStatus1Last.word = 0;
	chargeStatus2Last.word = 0;
	powerStatus.word[0] = 0;
	powerStatus.word[1] = 0;
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
	CHARGE_FACTOR_STATUS3 = 0;
	chargeCookieSetting = 0xc0;
	chargeTempSetting = 0xc0;
	chargeCookieHeld = 0;
	chargeManualMode = 0;
#if CHARGE_0104
	chargeSerialMode = 0;
#endif
	chargeUsbState = 0;
	chargeUsbWait = 0;
	chargeUsbTicks = 0;
	chargeAlarmInit();
	if (deviceVersions.cookie[0] != 0x80) {
		charge.batteryVolt = configGet()->batteryVolt;
		charge.batteryRemainCap = configGet()->batteryRemainCap;
		charge.batteryLifePercent = configGet()->batteryLifePercent;
		charge.batteryTemp = configGet()->batteryTemp;
		charge.batteryBicTemp = configGet()->batteryBicTemp;
		powerStatus.bits.battery = 1;
		powerStatus.bits.ac = 1;
		charge.flagsLast = 0;
		charge.flagsRead = 0;
		charge.flags = 0;
		sleepFactorSetStatus3(1);
		sleepFactorSetStatus4(0);
	}
	charge.timer1fPending = 0;
	handler.handler = chargeTimer;
	timerSetHandler(TIMER_RTC(0x17), &handler);
	handler.handler = chargeCookieTimer;
	timerSetHandler(TIMER_RTC(0x1e), &handler);
	handler.handler = chargeTimer1f;
	timerSetHandler(TIMER_RTC(0x1f), &handler);
	handler.handler = chargeUpdateSoon;
	timerSetHandler(CHARGE_SOON_TIMER, &handler);
#elif SOURCE_0103
	/* the slim's: the last status registers, the bytes more, and no AC supply setting */
	cookieStatus1.word = 0;
	cookieStatus2.word = 0;
	chargeStatus1Last.word = 0;
	chargeStatus2Last.word = 0;
	powerStatus.word[0] = 0;
	powerStatus.word[1] = 0;
	chargeTemp.setting1 = 0x60;
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
#if BOARD_USS1001 && SOURCE_0104
	chargeBoostTicks = 0;
	chargeBoost.byte = 0;
#endif
	chargeControl.0 = 0;
	chargeControl.1 = 1;
	CHARGE_FACTOR_STATUS3 = 0;
	chargeCookieSetting = 0x70;
	chargeTempSetting = 0x70;
	chargeCookieHeld = 0;
	chargeManualMode = 0;
#if CHARGE_0104
	chargeSerialMode = 0;
#endif
	chargeUsbState = 0;
	chargeUsbWait = 0;
	chargeUsbTicks = 0;
	chargeAlarmInit();
	if (deviceVersions.cookie[0] != 0x80) {
		charge.batteryVolt = configGet()->batteryVolt;
		charge.batteryRemainCap = configGet()->batteryRemainCap;
		charge.batteryLifePercent = configGet()->batteryLifePercent;
		charge.batteryTemp = configGet()->batteryTemp;
		charge.batteryBicTemp = configGet()->batteryBicTemp;
		powerStatus.bits.battery = 1;
		powerStatus.bits.ac = 0;
		charge.flagsLast = 0;
		charge.flagsRead = 0;
		charge.flags = 0;
		sleepFactorSetStatus3(1);
		sleepFactorSetStatus4(0);
	} else {
		chargeResetBattery();
		sleepFactorSetStatus3(0);
		sleepFactorSetStatus4(0);
		cookieCtrlCharge(1);
		if (deviceVersions.battery.id != 0) {
			batteryReadRemainCap(&charge.batteryRemainCap);
			batteryReadVolt(&charge.batteryVolt);
			charge.flagsRead = charge.flags = charge.flagsLast = chargeReadFlags();
#if BOARD_EVENT_LOG
			eventLogCharge.word[0] = charge.flags;
			eventLogCharge.word[1] = charge.batteryRemainCap;
			eventLogRecord(5, 1, eventLogCharge.byte, 4);
#endif
			CHARGE_FACTOR_19 = (charge.flags & 0x0200) ? 1 : 0;
			sleepFactorSet19(CHARGE_FACTOR_19);
			CHARGE_FACTOR_16 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(CHARGE_FACTOR_16);
			CHARGE_FACTOR_17 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(CHARGE_FACTOR_17);
			CHARGE_FACTOR_18 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(CHARGE_FACTOR_18);
		}
		if (deviceVersions.battery.version[2] != 0)
			chargeSetBattery();
	}
	charge.timer1fPending = 0;
	handler.handler = chargeTimer;
	timerSetHandler(TIMER_RTC(0x17), &handler);
	handler.handler = chargeCookieTimer;
	timerSetHandler(TIMER_RTC(0x1e), &handler);
	handler.handler = chargeTimer1f;
	timerSetHandler(TIMER_RTC(0x1f), &handler);
	handler.handler = chargeUpdateSoon;
	timerSetHandler(CHARGE_SOON_TIMER, &handler);
#else
#if !FW_SINCE(0x00090206)
	/* 0x00090009: no levels yet, the defaults as constants, and any Cookie that answered */
	charge.flags = charge.flagsLast = charge.flagsRead = 0x8000;
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
#if FW_SINCE(0x00090009)
	chargeBbmcVersion = 0;
#endif
	chargeControl = 0;
	CHARGE_FACTOR_STATUS3 = 0;
	if (deviceVersions.cookie[1] == 0xff) {
		charge.batteryVolt = 4000;
		charge.batteryRemainCap = 1500;
#if FW_SINCE(0x00090009)
		charge.batteryLifePercent = 90;
#endif
		charge.batteryTemp = 0x0bd8;
		powerStatus.byte[0] |= 0x60;
#if FW_SINCE(0x00090009)
		charge.flagsLast = 0;
		charge.flagsRead = 0;
		charge.flags = 0;
#else
		charge.flags = charge.flagsLast = charge.flagsRead = 0;
#endif
		sleepFactorSetStatus3(1);
		sleepFactorSetStatus4(0);
	} else {
		charge.batteryVolt = 0;
		charge.batteryRemainCap = 0;
#if FW_SINCE(0x00090009)
		charge.batteryLifePercent = 0;
#endif
		charge.batteryTemp = 0;
		sleepFactorSetStatus3(0);
		sleepFactorSetStatus4(0);
		if (deviceVersions.cookie[1] < 0x31)
			cookieCtrlSupplyCurrentAc(0x60);
#if FW_SINCE(0x00090009)
		if (deviceVersions.battery.id != 0) {
			batteryReadRemainCap(&charge.batteryRemainCap);
			batteryReadVolt(&charge.batteryVolt);
			charge.flagsLast = chargeReadFlags();
		}
		if (deviceVersions.battery.version[2] != 0)
			chargeSetBattery();
#endif
	}
#if !FW_SINCE(0x00090009)
	/* 0x00080504 reads no battery here, and sets it also with no Cookie */
	if (deviceVersions.battery.version[2] != 0)
		chargeSetBattery();
#endif
#else
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
#if FW_SINCE(0x00090804)
	chargeControl.0 = 0;
	chargeControl.1 = 1;
#else
	chargeControl = 0;
#endif
	CHARGE_FACTOR_STATUS3 = 0;
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
#if FW_SINCE(0x0100060B)
			charge.flagsRead = charge.flags = charge.flagsLast = chargeReadFlags();
#else
			charge.flagsLast = chargeReadFlags();
#endif
		}
		if (deviceVersions.battery.version[2] != 0)
			chargeSetBattery();
	}
#endif
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
#if FW_SINCE(0x0100060B)
	handler.handler = chargeUpdateSoon;
	timerSetHandler(CHARGE_SOON_TIMER, &handler);
#endif
#endif
}

#if (BOARD_USS1001 && FW_SINCE(0x01030511)) || CHARGE_0104
/* the slim's (0x01030511 on) and DOL-1002 call stateTransition themselves */
#define chargeTransition(transition)	stateTransition(transition)
#elif SOURCE_0103	/* and the PS TV's */
/* 0x01030505's: stateTransition with the transition's low byte */
void chargeTransition(int transition)
{
	stateTransition((unsigned char)transition);
}
#else
void chargeTransition(int transition)
{
	STATE_TRANSITION_EVENT
	STATE_TRANSITION((unsigned char)transition);
}
#endif

/* task 5's event 0x20 */
#if BOARD_DOL1001
/* DOL-1001's, which task 5 does not call: the last status registers kept */
void chargeUpdate(void)
{
	chargeStatus1Last = cookieStatus1;
	chargeStatus2Last = cookieStatus2;
}
#elif SOURCE_0103
/* the slim's: the USB's settling checks, the power LED's causes, the battery's boost and its
   faults record, the last status registers kept */
void chargeUpdate(void)
{
	unsigned int changed;
	unsigned int state;
	unsigned int ticks;
#if FW_SINCE(0x01030511)
	union CookieStatus1 status1;
#endif

	timerGetRtcTicks(&ticks);
	if (deviceVersions.cookie[0] != 0x80)
		goto done;
	cookieReadStatus1(cookieStatus1.byte);
	cookieReadStatus2(cookieStatus2.byte);
	cookieReadReg3Value(cookieRegs[0].byte);
	if (chargeBoost.bits.done && !(cookieRegs[0].byte[1] & 0x10))
		chargeBoost.bits.done = 0;
	if (cookieStatus2.bits.usb & 1)
		;
	if (cookieStatus2.bits.usb & 2)
		;
	if (cookieStatus2.bits.usb & 4)
		;
	if (chargeUsbState.7) {
		if (chargeUsbWait != 0) {
			if ((chargeUsbState.0 && (!cookieStatus2.bits.vbus || !cookieStatus2.bits.usb)) ||
			    (!chargeUsbState.0 && (cookieStatus2.bits.vbus || cookieStatus2.bits.usb))) {
				timerStart(CHARGE_SOON_TIMER, chargeUsbTicks);
				chargeUsbWait--;
			} else {
				chargeUsbWait = 0;
				if (chargeUsbState.0) {
					timerStart(CHARGE_SOON_TIMER, 0x32);
					chargeUsbState.6 = 1;
				}
			}
		} else {
			chargeUsbState.6 = 0;
		}
		if (chargeUsbWait == 0 && !chargeUsbState.6) {
			chargeUsbState.7 = 0;
			timerStart(TIMER_RTC(0x17), charge.acDelay);
		}
	}
	if (chargeUsbState.7) {
		cookieStatus2.byte[1] &= 0xb1;
		cookieStatus1.bits.status1 = 0;
	}
	cookieReadReg3();
	state = stateGet();
	if (chargeStatus1Last.word != cookieStatus1.word || chargeStatus2Last.word != cookieStatus2.word) {
		chargeSetPowerStatus();
		if ((chargeStatus1Last.bits.ac ^ cookieStatus1.bits.ac) ||
		    (chargeStatus1Last.bits.charging ^ cookieStatus1.bits.charging) ||
		    (chargeStatus2Last.bits.vbus ^ cookieStatus2.bits.vbus))
			inputChanged();
		if (cookieStatus1.bits.level)
			;
	}
	if (chargeStatus1Last.bits.status1 ^ cookieStatus1.bits.status1) {
		if (cookieStatus1.bits.status1)
			powerLedSetFlags(0x10);
		else
			powerLedClearFlags(0x10);
	}
	if (cookieStatus2.bits.vbus) {
		if (chargeUsbMode == 0)
			chargeUsbMode = 1;
		chargeSetCurrent();
	} else {
		chargeUsbMode = 0;
		chargeUsbHold = 0;
	}
	if (powerStatus.bits.cookieLock0 || powerStatus.bits.cookieLock1 ||
	    (chargeUsbIs1() && !chargeVbus.1) || !chargeVbus.0) {
		if (cookieStatus1.bits.status3)
			powerCookieSet();
	} else if (chargeCurrent != 0xff && (cookieRegs[3].byte[0] & 0x08)) {
		powerCookieClear();
	}
	if (deviceVersions.battery.id == 0) {
		if (batteryReadInfo(&deviceVersions.battery.id) == 0) {
			batteryInfoRead();
			chargeSetBattery();
		} else {
			deviceVersions.battery.id = deviceVersions.battery.version[0] =
				deviceVersions.battery.version[1] = deviceVersions.battery.version[2] =
				deviceVersions.battery.version[3] = deviceVersions.battery.version[4] = 0;
			goto done;
		}
	} else if (deviceVersions.battery.id == 0x0520) {
		batteryReadTemp(&charge.batteryTemp);
		batteryReadBicTemp(&charge.batteryBicTemp);
		chargeTempUpdate(charge.batteryTemp, &chargeTemp);
		if (ticks - charge.checkTicks > configGet()->chargeTempInterval) {
			charge.checkTicks = ticks;
			chargeAlarmUpdate();
		}
	}
	if (powerStatus.bits.battery) {
		batteryReadRemainCap(&charge.batteryRemainCap);
		batteryReadVolt(&charge.batteryVolt);
		batteryReadLifePercent(&charge.batteryLifePercent);
		if (configGet()->batteryLowVolt > charge.batteryVolt)
			powerLedClearFlags(0x20);
		charge.flagsRead = chargeReadFlags();
		changed = ~(charge.flagsRead ^ charge.flagsLast) & (charge.flags ^ charge.flagsRead);
		if (changed != 0) {
			dis_dsp();
			charge.flags ^= changed;
			ena_dsp();
#if BOARD_EVENT_LOG
			eventLogCharge.word[0] = charge.flags;
			eventLogCharge.word[1] = charge.batteryRemainCap;
			eventLogRecord(5, 1, eventLogCharge.byte, 4);
#endif
			if (changed & 0x0200) {
				if ((charge.flags & 0x0200) && (state & 0x0008)) {
					powerOffWlan();
					powerBbmcCycle();
				}
				CHARGE_FACTOR_19 = (charge.flags & 0x0200) ? 1 : 0;
				sleepFactorSet19(CHARGE_FACTOR_19);
			}
			if (changed & 0x0008) {
				if (charge.flags & 0x0008)
					powerLedSetFlags(2);
				else
					powerLedClearFlags(2);
			}
			CHARGE_FACTOR_16 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(CHARGE_FACTOR_16);
			CHARGE_FACTOR_17 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(CHARGE_FACTOR_17);
			CHARGE_FACTOR_18 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(CHARGE_FACTOR_18);
			if (CHARGE_FACTOR_19 && state != STATE_RESUME) {
				if (input1Bit7Active())
					;
				powerLedSetAlarm(1);
			} else {
				powerLedSetAlarm(0);
			}
			if (charge.flags & 0x0020) {
				if (!(stateGetWakeMask() & 0x40) && !(state & 0x8000) && (state & 0x0008))
					chargeTransition(0x06);
				else if (!(state & 0x8000) && (state & 0x0028))
					chargeTransition(0x37);
			}
		}
		charge.flagsLast = charge.flagsRead;
		if ((charge.flags & 0x0040) && P7.5 && !P13.7)
			chargeTransition(0x40);
		if (state & 0x000c) {
			if (((configGet()->state.bits.offWakeUnmask22 && chargeExternalPower()) ||
			     chargeControl.0) && CHARGE_TEST(chargeMayStart()) && !CHARGE_TEST(chargeAlarm0On())) {
				chargeTransition(0x16);
				chargeControl.0 = 0;
				chargeTimer1fStop();
			}
			if (!configGet()->state.bits.chargeHold && chargeUsbIs1() &&
			    !(stateGetWakeMask() & 0x8000) && chargeUsbHold == 0 &&
			    (!chargeGetManualMode() || configGet()->charge.bits.manualCharge) &&
			    (!(charge.flags & 0x0004) || CHARGE_TEST(chargeMayStart())) &&
			    !powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1)
				chargeTransition(0x0f);
		} else if (state & 0x0020) {
			if (configGet()->state.bits.chargeTimer1f) {
				if (!chargeExternalPower() && !charge.timer1fPending)
					chargeTimer1fStart();
				else if (chargeExternalPower() && charge.timer1fPending)
					chargeTimer1fStop();
			}
		}
		if (CHARGE_TEST(chargeBbmcChanged()))
			chargeSetCurrent();
		charge.batteryTempC = charge.batteryTemp - 0x0aac;
		charge.batteryBicTempC = charge.batteryBicTemp - 0x0aac;
		if (ticks - charge.tempTicks >= 0x79)
			charge.tempTicks = ticks;
		if (!chargeStatus1Last.bits.battery) {
			batteryFaults.byte[0] |= 0x10;
#if BOARD_EVENT_LOG
			eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
			if (cookieStatus1.bits.charging && cookieStatus2.bits.boost) {
				chargeBoostTicks = ticks;
				chargeBoost.bits.running = 1;
			}
		}
		if (chargeBoost.bits.running) {
			if (ticks - chargeBoostTicks >= configGet()->chargeBoostTime * 2 ||
			    charge.batteryVolt >= configGet()->chargeBoostVolt) {
				cookieCtrlReg3Bit12(1);
				batteryFaults.byte[0] |= 0x20;
#if BOARD_EVENT_LOG
				eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
				chargeBoost.bits.running = 0;
				chargeBoost.bits.done = 1;
			}
		}
	} else if (chargeStatus1Last.bits.battery) {
		chargeResetBattery();
		if (!configGet()->start.bits.batteryNoRetry && !(state & 0x8000) && (state & 0x0028))
			chargeTransition(0x38);
		chargeBoost.bits.running = 0;
		batteryFaults.byte[0] &= ~0x10;
		batteryFaults.byte[0] &= ~0x20;
#if BOARD_EVENT_LOG
		eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
		logEvent(0x0e);
	}
#if FW_SINCE(0x01030511)
	cookieReadStatus1(status1.byte);
	if (cookieStatus1.bits.status1 ^ status1.bits.status1) {
		if (status1.bits.status1)
			powerLedSetFlags(0x10);
		else
			powerLedClearFlags(0x10);
	}
#endif
done:
	chargeStatus1Last = cookieStatus1;
	chargeStatus2Last = cookieStatus2;
}
#elif FW_SINCE(0x01000104)
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
#if FW_SINCE(0x0100060B)
		if (((unsigned char)status1.bits.ac ^ (unsigned char)cookieStatus1.bits.ac) ||
		    ((unsigned char)status1.bits.charging ^ (unsigned char)cookieStatus1.bits.charging) ||
		    ((unsigned char)status2.bits.vbus ^ (unsigned char)cookieStatus2.bits.vbus))
#elif FW_SINCE(0x01000109)
		if ((status1.bits.ac ^ cookieStatus1.bits.ac) ||
		    (status1.bits.charging ^ cookieStatus1.bits.charging) ||
		    (status2.bits.vbus ^ cookieStatus2.bits.vbus))
#else
		if ((status1.bits.ac ^ cookieStatus1.bits.ac) ||
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
#if FW_SINCE(0x00090009)
		batteryReadLifePercent(&charge.batteryLifePercent);
#endif
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
				CHARGE_FACTOR_19 = (charge.flags & 0x0200) ? 1 : 0;
				sleepFactorSet19(CHARGE_FACTOR_19);
			}
			CHARGE_FACTOR_16 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(CHARGE_FACTOR_16);
			CHARGE_FACTOR_17 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(CHARGE_FACTOR_17);
			CHARGE_FACTOR_18 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(CHARGE_FACTOR_18);
#if FW_SINCE(0x01000107)
#if FW_SINCE(0x0100060B)
			if (CHARGE_FACTOR_19 && state != STATE_RESUME) {
#else
			if (CHARGE_FACTOR_19) {
#endif
				if (input1Bit7Active())
					chargeLedSetBlink(1);
				powerLedSetAlarm(1);
			} else {
				chargeLedSetBlink(0);
				powerLedSetAlarm(0);
			}
#else
			if (CHARGE_FACTOR_19) {
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
			if (((configGet()->state.bits.offWakeUnmask22 && CHARGE_FACTOR_STATUS3) ||
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
				if (!CHARGE_FACTOR_STATUS3 && !charge.timer1fPending)
					chargeTimer1fStart();
				else if (CHARGE_FACTOR_STATUS3 && charge.timer1fPending)
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
#if FW_SINCE(0x00090009)
		batteryReadLifePercent(&charge.batteryLifePercent);
#endif
		charge.flagsRead = chargeReadFlags();
		changed = ~(charge.flagsRead ^ charge.flagsLast) & (charge.flags ^ charge.flagsRead);
		if (changed != 0) {
			dis_dsp();
			charge.flags ^= changed;
			ena_dsp();
			CHARGE_FACTOR_16 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(CHARGE_FACTOR_16);
			CHARGE_FACTOR_17 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(CHARGE_FACTOR_17);
			CHARGE_FACTOR_18 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(CHARGE_FACTOR_18);
			CHARGE_FACTOR_19 = (charge.flags & 0x0200) ? 1 : 0;
			sleepFactorSet19(CHARGE_FACTOR_19);
			if (CHARGE_FACTOR_19) {
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

#if FW_SINCE(0x00090206)
	if (deviceVersions.cookie[0] != 0x80)
#else
	if (deviceVersions.cookie[1] == 0xff)	/* 0x00090009: any Cookie that answered */
#endif
		return;
	status1 = cookieStatus1;
	status2 = cookieStatus2;
	reg = COOKIE_CTRL_REG(1);
	cookieRead(&reg, cookieStatus1.byte);
	reg = COOKIE_CTRL_REG(2);
	cookieRead(&reg, cookieStatus2.byte);
	state = stateGet();
	ac = CHARGE_FACTOR_STATUS3;
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
		CHARGE_FACTOR_STATUS3 = powerStatus.bits.ac ? 1 : (cookieStatus2.bits.usb & 2) ? 1 : 0;
		sleepFactorSetStatus3(CHARGE_FACTOR_STATUS3);
		if (CHARGE_FACTOR_STATUS3 != ac) {
			if (CHARGE_FACTOR_STATUS3) {
				charge.acDelay = 10;
				if (!(stateGetWakeMask() & 0x00400000))
					CHARGE_AC_DUE = 1;
			} else {
				CHARGE_AC_DUE = 0;
				charge.acDelay = 0x3c;
			}
			timerStart(TIMER_RTC(0x17), charge.acDelay);
		}
		if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && (cookieStatus1.byte[1] & 0xc0) == 0x80)
			CHARGE_FACTOR_STATUS4 = 1;
		else
			CHARGE_FACTOR_STATUS4 = 0;
		sleepFactorSetStatus4(CHARGE_FACTOR_STATUS4);
		ena_dsp();
		if (cookieStatus2.bits.vbusCharge) {
#if FW_SINCE(0x00090804)
			if (chargeUsbMode == 0 && !P14.5 && (cookieStatus2.bits.usb & 2)) {
				chargeUsbMode = 1;
				chargeSetCurrent();
			}
#else
			if (!(status2.word & 0x8000) && (cookieStatus2.bits.usb & 2)) {
				if (chargeUsbMode == 0)
					chargeUsbMode = 1;
				chargeSetCurrent();
			}
#endif
		} else if (status2.word & 0x8000) {
			if (deviceVersions.cookie[1] < 0x31)
				cookieCtrlSupplyCurrentUsb(0x10);
		}
		if (cookieStatus1.bits.level)
			;
	}
#if FW_SINCE(0x00090804)
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
#endif
	if (powerStatus.bits.battery) {
#if FW_SINCE(0x00090804)
		if (!(status1.word & 0x0002) && deviceVersions.battery.version[1] >= 0x0408 &&
		    deviceVersions.battery.version[1] < 0x0412)
			return;
#else
		if (deviceVersions.battery.id == 0) {
			if (batteryReadInfo(&deviceVersions.battery.id) == 0) {
				chargeSetBattery();
			} else {
				deviceVersions.battery.id = deviceVersions.battery.version[0] =
					deviceVersions.battery.version[1] =
					deviceVersions.battery.version[2] = 0;
				return;
			}
		}
#endif
		batteryReadRemainCap(&charge.batteryRemainCap);
		batteryReadVolt(&charge.batteryVolt);
#if FW_SINCE(0x00090009)
		batteryReadLifePercent(&charge.batteryLifePercent);
#endif
		charge.flagsRead = chargeReadFlags();
		changed = ~(charge.flagsRead ^ charge.flagsLast) & (charge.flags ^ charge.flagsRead);
		if (changed != 0) {
			dis_dsp();
			charge.flags ^= changed;
			ena_dsp();
			CHARGE_FACTOR_16 = (charge.flags & 0x0010) ? 1 : 0;
			sleepFactorSet16(CHARGE_FACTOR_16);
			CHARGE_FACTOR_17 = (charge.flags & 0x0008) ? 1 : 0;
			sleepFactorSet17(CHARGE_FACTOR_17);
			CHARGE_FACTOR_18 = (charge.flags & 0x0100) ? 1 : 0;
			sleepFactorSet18(CHARGE_FACTOR_18);
#if FW_SINCE(0x00090206)	/* 0x00090009: no factor 5, no blink */
			CHARGE_FACTOR_19 = (charge.flags & 0x0200) ? 1 : 0;
			sleepFactorSet19(CHARGE_FACTOR_19);
			if (CHARGE_FACTOR_19) {
				if (!chargeLedBlinking && input1Bit7Active())
					chargeLedSetBlink(1);
			} else if (chargeLedBlinking) {
				chargeLedSetBlink(0);
			}
#endif
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
			if (CHARGE_AC_DUE && CHARGE_TEST(chargeMayStart())) {
				chargeTransition(0x16);
				CHARGE_AC_DUE = 0;
			}
#if FW_SINCE(0x00090804)
			if ((cookieStatus2.byte[1] & 0x0c) == 0x04 && !(stateGetWakeMask() & 0x8000) &&
			    (!(charge.flags & 0x0004) || CHARGE_TEST(chargeMayStart())))
#else
			if ((cookieStatus2.bits.usb & 1) && !(stateGetWakeMask() & 0x8000) &&
			    (!(charge.flags & 0x0004) || CHARGE_TEST(chargeMayStart())))
#endif
				chargeTransition(0x0f);
		}
#if !FW_SINCE(0x00090206)
		/* 0x00090009: the temperature in degrees C first, no BIC temperature */
		batteryReadTemp(&charge.batteryTemp);
		charge.batteryTempC = charge.batteryTemp - 0x0aac;
		chargeTempUpdate(charge.batteryTemp, &chargeTemp);
#elif !FW_SINCE(0x00090804)
		batteryReadTemp(&charge.batteryTemp);
		batteryReadBicTemp(&charge.batteryBicTemp);
		chargeTempUpdate(charge.batteryTemp, &chargeTemp);
#endif
#if FW_SINCE(0x00090009)	/* 0x00080504 has no 3G modem check */
		if (CHARGE_TEST(chargeBbmcChanged()))
			chargeSetCurrent();
#endif
#if FW_SINCE(0x00090206)
		charge.batteryTempC = charge.batteryTemp - 0x0aac;
		charge.batteryBicTempC = charge.batteryBicTemp - 0x0aac;
#endif
#if FW_SINCE(0x00090009)
		timerGetRtcTicks(&charge.checkTicks);
		if (charge.checkTicks - charge.tempTicks >= 0x79)
			charge.tempTicks = charge.checkTicks;
#else
		timerGetRtcTicks(&chargeCheckTicks);
		if (chargeCheckTicks - chargeTempTicks >= 0x79)
			chargeTempTicks = chargeCheckTicks;
#endif
	}
#if FW_SINCE(0x00090206)	/* 0x00090009 leaves the readings when the battery goes */
	else if (status1.word & 0x0002) {
		chargeResetBattery();
		if (!configGet()->start.bits.batteryNoRetry && !(state & 0x8000) && (state & 0x0028))
			chargeTransition(0x32);
#if FW_SINCE(0x00090804)
		logEvent(0x0e);
#endif
	}
#endif
}
#endif

#if BOARD_DOL1001
/* DOL-1001 has no chargeSetPowerStatus, chargeReadFlags or chargeTempUpdate */
#elif SOURCE_0103
/* the slim's: the USB's three bits, the supply's and VBUS, the AC factor also from a USB charger,
   and a log record at each change */
void chargeSetPowerStatus(void)
{
	unsigned char ac;
	union PowerStatus status;

	status.word[0] = powerStatus.word[0];
	status.word[1] = powerStatus.word[1];
	ac = CHARGE_FACTOR_STATUS3;
	dis_dsp();
	powerStatus.bits.charging = cookieStatus1.bits.charging;
	powerStatus.bits.battery = cookieStatus1.bits.battery;
	powerStatus.bits.ac = cookieStatus1.bits.ac;
	powerStatus.bits.status3 = cookieStatus1.bits.status3;
	powerStatus.bits.status2 = cookieStatus1.bits.status2;
	powerStatus.bits.status1 = cookieStatus1.bits.status1;
	powerStatus.bits.level = cookieStatus1.bits.level;
	powerStatus.bits.usb = cookieStatus2.bits.usb;
#if CHARGE_0104
	/* the slim's 0x0105: a USB host in the serial operation's mode is a charger */
	if (chargeSerialMode != 0 && powerStatus.bits.usb == 1)
		powerStatus.bits.usb = 3;
#endif
	powerStatus.bits.supply = cookieStatus2.bits.supply;
	powerStatus.bits.vbus1 = powerStatus.bits.vbus2 = cookieStatus2.bits.vbus;
	if (powerStatus.bits.usb)
		;
	CHARGE_FACTOR_STATUS3 = powerStatus.bits.ac ? 1 : chargeUsbPower() ? 1 : 0;
	sleepFactorSetStatus3(CHARGE_FACTOR_STATUS3);
	if (ac != CHARGE_FACTOR_STATUS3) {
		if (CHARGE_FACTOR_STATUS3) {
#if STATE_WAKE_BITS
			if (!stateGetWakeBit(0x16))
#else
			if (!(stateGetWakeMask() & 0x00400000))
#endif
				chargeControl.0 = 1;
		} else {
			chargeControl.0 = 0;
		}
		chargeSetAcDelay();
		timerStart(TIMER_RTC(0x17), charge.acDelay);
	}
	if (chargeUsbIs1() && (cookieStatus1.byte[1] & 0xc0) == 0x80)
		CHARGE_FACTOR_STATUS4 = 1;
	else
		CHARGE_FACTOR_STATUS4 = 0;
	sleepFactorSetStatus4(CHARGE_FACTOR_STATUS4);
	ena_dsp();
#if BOARD_EVENT_LOG
	if (status.word[0] != powerStatus.word[0] || status.word[1] != powerStatus.word[1])
		eventLogRecord(5, 0x1f, powerStatus.byte, 4);
#endif
}
#elif FW_SINCE(0x00090903)
/* the power status and the charge's sleep factors from the Cookie's status registers */
void chargeSetPowerStatus(void)
{
	unsigned char ac;

	ac = CHARGE_FACTOR_STATUS3;
	dis_dsp();
	powerStatus.bits.charging = cookieStatus1.bits.charging;
	powerStatus.bits.battery = cookieStatus1.bits.battery;
	powerStatus.bits.ac = cookieStatus1.bits.ac;
	powerStatus.bits.status3 = cookieStatus1.bits.status3;
	powerStatus.bits.status2 = cookieStatus1.bits.status2;
	powerStatus.bits.status1 = cookieStatus1.bits.status1;
	powerStatus.bits.level = cookieStatus1.bits.level;
	powerStatus.bits.usb = cookieStatus2.bits.usb;
	CHARGE_FACTOR_STATUS3 = powerStatus.bits.ac ? 1 : (cookieStatus2.bits.usb & 2) ? 1 : 0;
	sleepFactorSetStatus3(CHARGE_FACTOR_STATUS3);
	if (ac != CHARGE_FACTOR_STATUS3) {
		if (CHARGE_FACTOR_STATUS3) {
#if !FW_SINCE(0x01000306)
			charge.acDelay = 10;
#endif
			if (!(stateGetWakeMask() & 0x00400000))
				chargeControl.0 = 1;
		} else {
			chargeControl.0 = 0;
#if !FW_SINCE(0x01000104) || FW_RETAIL_10X && BOARD_IS(0x00406000)
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
		CHARGE_FACTOR_STATUS4 = 1;
	else
		CHARGE_FACTOR_STATUS4 = 0;
	sleepFactorSetStatus4(CHARGE_FACTOR_STATUS4);
	ena_dsp();
}
#endif

#if !BOARD_DOL1001
/* the battery's flags: bit n set when the battery is at or below level n */
unsigned int chargeReadFlags(void)
{
	unsigned char capBelow;
	unsigned char voltBelow;
	unsigned int flags;
	unsigned char i;

	flags = 0;
	paramGetChargeLevels(chargeLevels);
	for (i = 0; i < CHARGE_LEVELS; i++) {
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
#if SOURCE_0103
	unsigned int setting;
#endif
	struct ChargeTempLevel *levels;

	i = 0;
#if FW_SINCE(0x00090009)
	if (stateGet() == 0x0020)
		levels = configGet()->chargeTempLevelsCharge;
	else
		levels = configGet()->chargeTempLevels;
#else
	if (stateGet() == 0x0020)
		levels = (struct ChargeTempLevel *)chargeTempLevelsCharge;
	else
		levels = (struct ChargeTempLevel *)chargeTempLevels;
#endif
	if (t->temp < temp)
		cooling = 0;
	else
		cooling = 1;
	for (i = 0; i < CHARGE_TEMP_LEVELS; i++) {
		if (temp < levels[i].limit[cooling]) {
			t->read = i;
			break;
		}
	}
	if (t->read == t->readLast) {
		if (t->read != t->level) {
			t->level = t->read;
			t->temp = temp;
#if BOARD_EVENT_LOG
			eventLogCharge.byte[0] = t->level;
			eventLogCharge.byte[1] = cooling;
			eventLogCharge.word[1] = temp;
			eventLogRecord(5, 2, eventLogCharge.byte, 4);
#endif
		}
#if SOURCE_0103
		/* the slim's: the USB supply's setting the range allows */
		setting = chargeTempSetting;
		if (t->level >= 4)
			chargeTempSetting = 0x30;
		else
			chargeTempSetting = 0x70;
		if (chargeTempSetting != setting)
			chargeSetCurrent();
#endif
		if (t->setting1 != levels[t->level].setting1) {
			if (levels[t->level].setting1 == 0xff) {
				cookieCtrlCharge(0);
#if FW_SINCE(0x00090206)	/* 0x00090009 sets no Cookie locks */
				if (t->level == 0) {
					powerStatus.bits.cookieLock0 = 1;
#if SOURCE_0103
					if (configGet()->charge.bits.manualOffCold)
						chargeSetManualMode(0);
#endif
#if FW_SINCE(0x00090804)
					logEvent(9);
#endif
				} else {
					powerStatus.bits.cookieLock1 = 1;
#if FW_SINCE(0x00090804)
					logEvent(0x0a);
#endif
				}
#endif
			} else {
				if (t->setting1 == 0xff) {
#if FW_SINCE(0x00090206)
					powerStatus.byte[1] &= 0xcf;
#endif
					cookieCtrlCharge(1);
				}
#if SOURCE_0103
				cookieUpdateChargingCurrent(levels[t->level].setting1);
#else
				cookieCtrlChargingCurrent(levels[t->level].setting1);
#endif
			}
			t->setting1 = levels[t->level].setting1;
		}
		if (levels[t->level].setting2 != 0xff && t->setting2 != levels[t->level].setting2) {
#if SOURCE_0103
			cookieUpdateReg8(levels[t->level].setting2);
#else
			cookieCtrlReg8(levels[t->level].setting2);
#endif
			t->setting2 = levels[t->level].setting2;
		}
	}
	t->readLast = t->read;
}

#endif /* !BOARD_DOL1001 */

#if FW_SINCE(0x00090903)
/* the battery temperature's alarms: each one's handler runs when its temperature has stayed
   past its limits, or back, for the limit's time */
void chargeAlarmUpdate(void)
{
	unsigned int temp;
	unsigned int hold;
	unsigned int ticks;
#if FW_SINCE(0x0100060B)
	int adjust;
	unsigned char level;
	unsigned char i;
	struct ChargeAlarmLimit *limits;

	if (configGet()->board.bits.bbmcDetect) {
		paramGetAlarmLevel3G(&level);
		limits = configGet()->chargeAlarmLimits3G;
	} else {
		paramGetAlarmLevel(&level);
		limits = configGet()->chargeAlarmLimits;
	}
#elif FW_SINCE(0x01000104)
	unsigned char i;
	struct ChargeAlarmLimit *limits;

	if (configGet()->board.bits.bbmcDetect)
		limits = configGet()->chargeAlarmLimits3G;
	else
		limits = configGet()->chargeAlarmLimits;
#else
	unsigned char i;

	/* before 0x01000104 no 3G modem's limits, and the config is looked up each time */
#define limits	(configGet()->chargeAlarmLimits)
#endif
	for (i = 0; i < 4; i++) {
#if BOARD_DOL1001
		temp = charge.thermalTemp;	/* DOL-1001: the temperature sensor's */
#else
		if (limits[i].bicTemp == 0)
			temp = charge.batteryTemp;
		else
			temp = charge.batteryBicTemp;
#endif
#if FW_SINCE(0x0100060B)
		adjust = configGet()->chargeAlarmAdjust[level][i];
		if (temp >= limits[i].high + adjust)
			charge.alarms[i].flags.bits.over = 1;
		else if (limits[i].low + adjust >= temp)
			charge.alarms[i].flags.bits.over = 0;
#else
		if (temp >= limits[i].high)
			charge.alarms[i].flags.bits.over = 1;
		else if (limits[i].low >= temp)
			charge.alarms[i].flags.bits.over = 0;
#endif
#if SOURCE_0103
		else if (charge.alarms[i].flags.bits.on)
			charge.alarms[i].flags.bits.over = 1;
		else
			charge.alarms[i].flags.bits.over = 0;
#else
		else
			charge.alarms[i].flags.bits.over = charge.alarms[i].flags.bits.on;
#endif
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

#if BOARD_DOL1001
/* DOL-1001's alarms 0 and 2: an event in the log while on */
static void chargeAlarm0(void)
{
	if (charge.alarms[0].flags.bits.on)
		logEvent(0x0b);
}
#else
/* alarm 0's: sleep factor 23, and timer 0x1d */
static void chargeAlarm0(void)
{
	if (charge.alarms[0].flags.bits.on) {
		charge.alarmsOn.bits.alarm0 = 1;
		timerStart(TIMER_RTC(0x1d), configGet()->chargeAlarm0Delay);
#if SOURCE_0103
		/* the slim's: the manual charge mode off with it */
		if (configGet()->charge.bits.manualOffCold)
			chargeSetManualMode(0);
#endif
	} else {
		charge.alarmsOn.bits.alarm0 = 0;
	}
	CHARGE_FACTOR_23 = charge.alarms[0].flags.bits.on;
	sleepFactorSet23(CHARGE_FACTOR_23);
}
#endif

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
	CHARGE_FACTOR_22 = charge.alarms[1].flags.bits.on;
	sleepFactorSet22(CHARGE_FACTOR_22);
}

#if BOARD_DOL1001
static void chargeAlarm2(void)
{
	if (charge.alarms[2].flags.bits.on)
		logEvent(0x0a);
}
#else
/* alarm 2's: the Cookie's setting cookieCtrlSupplyCurrentAc */
static void chargeAlarm2(void)
{
	if (charge.alarms[2].flags.bits.on) {
		charge.alarmsOn.bits.alarm2 = 1;
		chargeCookieSetting = configGet()->chargeAlarm2Setting;
	} else {
		charge.alarmsOn.bits.alarm2 = 0;
#if SOURCE_0103
		chargeCookieSetting = 0x70;
#else
		chargeCookieSetting = 0xc0;
#endif
	}
	chargeSetCurrent();
#if SOURCE_0103
	cookieUpdateSupplyCurrent(chargeCookieSetting);
#else
	cookieCtrlSupplyCurrentAc(chargeCookieSetting);
#endif
}
#endif

#if BOARD_USS1001 && SOURCE_0104
/* the slim's 0x0105: alarm 3, held on from the start, off once its temperature is reached; on, it
   moves state 0x20 on (transition 0x3b) */
static void chargeAlarm3(void)
{
	if (charge.alarms[3].flags.bits.on) {
		charge.alarmsOn.bits.alarm3 = 0;
	} else {
		charge.alarmsOn.bits.alarm3 = 1;
		if (stateGet() == 0x20)
			stateTransition(0x3b);
	}
}
#else
static void chargeAlarm3(void)
{
}
#endif

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
#if BOARD_DOL1001
	charge.alarms[1].handler = chargeAlarm3;	/* DOL-1001: alarm 1 does nothing */
#else
	charge.alarms[1].handler = chargeAlarm1;
#endif
	charge.alarms[2].flags.byte = 0;
	charge.alarms[2].since = 0;
	charge.alarms[2].handler = chargeAlarm2;
	charge.alarms[3].flags.byte = 0;
	charge.alarms[3].since = 0;
	charge.alarms[3].handler = chargeAlarm3;
#if BOARD_USS1001 && SOURCE_0104
	charge.alarms[3].flags.byte |= 0x07;	/* on, and over at the last checks */
#endif
	handler.handler = chargeAlarm0Timer;
	timerSetHandler(TIMER_RTC(0x1d), &handler);
}
#endif

#if SOURCE_0103
/* the slim's: an update, unless the USB is settling */
int chargeUsbRequestUpdate(void)
{
	if (!chargeUsbState.7)
		task5SetFlag(4, 0x0020);
	return 0;
}
#endif

/* timer 0x17: the next update, at once while the battery's flag 15 is set */
void chargeTimer(void)
{
	if (charge.flags & 0x8000)
		timerStart(TIMER_RTC(0x17), 1);
	else
		timerStart(TIMER_RTC(0x17), charge.acDelay);
	task5ISetFlag(0x0020);
}

#if FW_SINCE(0x01000104)
/* timer 0x1e (powerCookieHold): task 5 runs powerCookieRelease (the slim's: nothing) */
void chargeCookieTimer(void)
{
#if !BOARD_USS1001	/* the slim's: nothing */
	task5SetFlag(4, 0x0400);
#endif
}

/* timer 0x1f */
void chargeTimer1f(void)
{
	if (!(stateGet() & 0x0004))
		chargeTransition(0x3a);
	charge.timer1fPending = 0;
}
#endif

#if FW_SINCE(0x0100060B)
/* the charge input's handler: chargeUpdateSoon after 5 ticks of timer 0x16 */
void chargeUpdateLater(void)
{
#if !SOURCE_0103	/* the slim's: nothing */
	timerStart(CHARGE_SOON_TIMER, 5);
#endif
}
#endif

void chargeUpdateSoon(void)
{
#if FW_SINCE(0x0100060B)
	/* timer 0x16's handler: the wait is the timer's */
#elif FW_SINCE(0x00090903)
	dly_tsk(5);
#elif FW_SINCE(0x00090804)
	if (!input1Bit7Active())
		chargeUsbMode = 0;
#endif
	task5ISetFlag(0x0020);
#if FW_SINCE(0x00090206) && !BOARD_DOL1001	/* 0x00090009 and the PS TV have no flag 0x1000 */
	task5ISetFlag(0x1000);
#endif
}

#if SOURCE_0103
/* task 5's event 0x0400 on the slim: the USB connected or not (P7.5 low), the status
   registers taken as the USB's, and its checks started */
#if BOARD_DOL1001
void chargeUsbChanged(void)
{
	/* DOL-1001: nothing */
}
#else
void chargeUsbChanged(void)
{
	chargeUsbState.0 = !P7.5 ? 1 : 0;
	chargeTimerStop();
	chargeUsbState.7 = 1;
	chargeUsbState.6 = 0;
	cookieStatus1.bits.charging = chargeUsbState.0;
	cookieStatus2.byte[1] &= 0xb1;
	cookieStatus1.bits.status1 = 0;
	chargeSetPowerStatus();
	inputChanged();
	if (chargeUsbState.0) {
		stateTransition(0x62);
#if BOARD_USS1001 && SOURCE_0104
		chargeUsbWait = 0x13;	/* the slim's 0x0105: the timer in ticks */
#else
		chargeUsbWait = 0x14;
#endif
		chargeUsbTicks = 0x19;
	} else {
		chargeUsbMode = 0;
		chargeUsbHold = 0;
		chargeBoost.bits.running = 0;
#if CHARGE_0104
		chargeSerialMode = 0;
		chargeUsbWait = 3;
#else
		chargeUsbWait = 4;
#endif
		chargeUsbTicks = 3;
	}
#if BOARD_USS1001 && SOURCE_0104
	timerStart(CHARGE_SOON_TIMER, chargeUsbTicks);
#else
	timerStart(CHARGE_SOON_TIMER, (chargeUsbTicks >> 2) + ((chargeUsbTicks & 3) ? 1 : 0));
#endif
}
#endif

/* the slim's USB supply setting: the multi-connector's, or the USB status's and
   chargeUsbMode's */
unsigned char chargeSetCurrent(void)
{
#if (FW_SINCE(0x01030511) && !BOARD_DOL1001) || CHARGE_0104
	unsigned char current;
	unsigned char result;
#else
	unsigned char result;
	unsigned char current;
#endif

#if (!FW_SINCE(0x01030511) || BOARD_DOL1001) && !CHARGE_0104
	unsigned char adjust;
#endif

	current = 0x10;
#if CHARGE_0104
	/* DOL-1002's: the dongle's and debug board's setting from the settings unless the serial
	   mode is set, a level 2 multi-connector's fixed, the USB status's in chargeUsbCurrent */
	if (chargeUsbDongle() || chargeUsbDebug()) {
		if (chargeSerialMode != 0)
			current = chargeUsbCurrent();
		else
			current = configGet()->chargeDongleCurrent;
	} else if (chargeUsbMultiCn2()) {
		current = BOARD_DOL1001 ? 0xc0 : 0x70;	/* the slim's 0x0105: 0x70 */
	} else {
		current = chargeUsbCurrent();
	}
#elif BOARD_DOL1001
	/* DOL-1001's: as 0x01030505's, from the older table (chargeCurrents), its dongle's and debug
	   board's settings fixed */
	if (chargeUsbDongle()) {
		current = 0x50;
	} else if (chargeUsbDebug()) {
		current = 0xc0;
	} else if (powerStatus.bits.usb < 4 && chargeUsbMode < 6) {
		current = configGet()->chargeCurrents[powerStatus.bits.usb][chargeUsbMode];
		if (current == 0xff || (chargeUsbIs1() && !chargeVbus.1) || !chargeVbus.0) {
			current = 0xff;
		} else if (chargeBbmcVersion == 0 && chargeUsbPower() && current >= 0x11) {
			paramGetAdjust(&adjust);
			current -= adjust;
		}
	}
#elif FW_SINCE(0x01030511)
	if (chargeUsbDongle() || chargeUsbDebug())
		current = configGet()->chargeDongleCurrent;
	else if (chargeUsbMultiCn2())
		current = 0x70;
	else
		current = chargeUsbCurrent();
#else
	/* 0x01030505: the dongle's setting fixed, no debug board, the USB status's inline */
	if (chargeUsbDongle()) {
		current = 0x30;
	} else if (chargeUsbMultiCn2()) {
		current = 0x70;
	} else if (powerStatus.bits.usb < 8 && chargeUsbMode < 6) {
		current = configGet()->chargeUsbCurrents[powerStatus.bits.usb][chargeUsbMode];
		if (current == 0xff || (chargeUsbIs1() && !chargeVbus.1) || !chargeVbus.0) {
			current = 0xff;
		} else if (chargeBbmcVersion == 0 && chargeUsbPower() && current >= 0x11) {
			paramGetAdjust(&adjust);
			current -= adjust;
		}
	}
#endif
	chargeCurrent = current;
	if (current == 0xff) {
		result = powerCookieSet();
	} else {
		result = cookieUpdateSupplyCurrentUsb(current);
#if BOARD_DOL1001
		if (result == 0 && (cookieRegs[0].byte[0] & 0x80))
#else
		if (result == 0 && (cookieRegs[3].byte[0] & 0x08))
#endif
			result = powerCookieClear();
	}
	return result;
}

#if (FW_SINCE(0x01030511) && !BOARD_DOL1001) || CHARGE_0104
unsigned char chargeUsbCurrent(void)
{
	unsigned char current;
	unsigned char adjust;

	current = 0x10;
#if BOARD_DOL1002
	/* DOL-1002's: DOL-1001's table */
	if (powerStatus.bits.usb < 4 && chargeUsbMode < 6) {
		current = configGet()->chargeCurrents[powerStatus.bits.usb][chargeUsbMode];
#else
	if (powerStatus.bits.usb < 8 && chargeUsbMode < 6) {
		current = configGet()->chargeUsbCurrents[powerStatus.bits.usb][chargeUsbMode];
#endif
		if (current == 0xff || (chargeUsbIs1() && !chargeVbus.1) || !chargeVbus.0) {
			current = 0xff;
		} else if (chargeBbmcVersion == 0 && chargeUsbPower() && current >= 0x11) {
			paramGetAdjust(&adjust);
			current -= adjust;
		}
	}
	return current;
}
#endif
#elif FW_SINCE(0x01000104)
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
#elif !FW_SINCE(0x00090009)
/* 0x00080504's: the table near, no limits looked at, no 3G modem adjustment, and the result
   not set when the setting stays 0xff */
static const unsigned char chargeCurrents[4][5] = {
	{ 0x10, 0x10, 0x10, 0x10, 0xff },
	{ 0x10, 0x10, 0x10, 0x50, 0xff },
	{ 0x10, 0xc0, 0xc0, 0xc0, 0xc0 },
	{ 0x10, 0xc0, 0xc0, 0xc0, 0xc0 },
};

int chargeSetCurrent(void)
{
	unsigned char result;
	unsigned char setting;

	setting = chargeCurrents[powerStatus.bits.usb][chargeUsbMode];
	if (setting == 0xff) {
		if (chargeCurrent != 0xff) {
			chargeCurrent = 0xff;
			result = powerCookieSet();
		}
	} else {
		if ((cookieRegs[0].byte[0] & 0x80) && chargeVbus)
			result = powerCookieClear();
		if (deviceVersions.cookie[1] < 0x31 && setting >= 0x11)
			setting = (setting >> 1) & 0xf0;
		chargeCurrent = setting;
		result = cookieCtrlSupplyCurrentUsb(setting);
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
#if FW_SINCE(0x00090804)
			if (cookieRegs[0].byte[0] & 0x80)
#else
			if ((cookieRegs[0].byte[0] & 0x80) && chargeVbus)
#endif
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
		if (chargeBatteryLevel0() && !CHARGE_FACTOR_STATUS3)
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
#if BOARD_USS1001 && SOURCE_0104
	if (charge.alarmsOn.byte & 0x09)	/* the slim's 0x0105: or alarm 3 */
#else
	if (charge.alarmsOn.byte & 0x01)
#endif
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

#if SOURCE_0103
char chargeBatteryLevel3(void)
{
	return (charge.flags & 0x0008) ? 1 : 0;
}
#endif
#else
/* before 0x01000104 alarm 0 is looked at here (0x00090804 has none), and the battery's
   levels whatever the state */
int chargeMayStart(void)
{
#if FW_SINCE(0x00090903)
	if (charge.alarmsOn.byte & 0x01)
		return 0;
#endif
#if FW_SINCE(0x00090009)	/* 0x00080504 looks only at the levels */
	if (configGet()->start.bits.batteryNoRetry)
		return 1;
#endif
#if FW_SINCE(0x00090804)
	if (deviceVersions.battery.id == 0xfffe)
		return 1;
#endif
	if (charge.flags & 0x8000)
		return 0;
	if (charge.flags & 0x0002)
		return 0;
	if ((charge.flags & 0x0001) && !CHARGE_FACTOR_STATUS3)
		return 0;
	return 1;
}

#if FW_SINCE(0x00090206)	/* 0x00090009 has nine levels */
char chargeBatteryLevel9(void)
{
	return (charge.flags & 0x0200) ? 1 : 0;
}
#endif
#endif

#if CHARGE_0104
/* DOL-1002's, which nothing calls: the battery charged enough, its remaining capacity 400 and its
   voltage 3500 (with a USB charger: any) */
int chargeBatteryReady(void)
{
	int ready;

	ready = 0;
	if (chargeUsbPower())
		ready = 1;
	else if (charge.batteryRemainCap >= 0x190 && charge.batteryVolt >= 0xdac)
		ready = 1;
	return ready;
}

/* and with a USB charger a remaining capacity of 320 */
int chargeBatteryReadyUsb(void)
{
	int ready;

	ready = 0;
	if (chargeUsbPower()) {
		if (charge.batteryRemainCap >= 0x140 && charge.batteryVolt >= 0xdac)
			ready = 1;
	} else if (charge.batteryRemainCap >= 0x190 && charge.batteryVolt >= 0xdac) {
		ready = 1;
	}
	return ready;
}

/* the battery's life in four bars, none lit at level 3 */
void chargeGetBatteryBars(struct ChargeBatteryBars *out)
{
	unsigned int percent;
	unsigned char bars;

	percent = charge.batteryLifePercent;
	bars = 0;
	if (chargeBatteryLevel3())
		bars = 0;
	else if (percent < 0x23)
		bars = 1;
	else if (percent < 0x37)
		bars = 2;
	else if (percent < 0x46)
		bars = 3;
	else
		bars = 4;
	out->count = 4;
	out->lit = bars;
}

/* the Cookie's battery setting for the battery's version: the settings' for a version it does
   not know, none with no battery */
void chargeSetBattery(void)
{
	unsigned char setting;

	setting = configGet()->batterySetting;
	if (deviceVersions.battery.id != 0xffff) {
#if BOARD_USS1001
		/* the slim's 0x0105: 0x60 for a battery version below 0x100, else the setting's */
		switch (deviceVersions.battery.version[2] & 0xff00) {
		case 0x0000:
			setting = 0x60;
			break;
		}
#else
		switch (deviceVersions.battery.version[2] & 0xfff0) {
		case 0x00:
		case 0x10:
		case 0x20:
			setting = 0x20;
			break;
		case 0x30:
			setting = 0x40;
			break;
		case 0x40:
		case 0x50:
			setting = 0x60;
			break;
		case 0x60:
			setting = 0xc0;
			break;
		}
#endif
		cookieCtrlReg7(setting);
	}
}
#else
/* the Cookie's battery setting for the battery's version */
void chargeSetBattery(void)
{
#if SOURCE_0103
	/* the slim's: none with no battery answered */
	if (deviceVersions.battery.id == 0xffff)
		return;
#endif
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
#endif

#if FW_SINCE(0x00090009)	/* 0x00080504 has none */
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
#endif

#if FW_SINCE(0x00090206)	/* 0x00090009 has none */
/* no battery */
void chargeResetBattery(void)
{
	charge.flags = charge.flagsLast = charge.flagsRead = 0x8000;
	charge.batteryBicTempC = charge.batteryTempC = charge.batteryBicTemp = charge.batteryTemp =
		charge.batteryLifePercent = charge.batteryRemainCap = charge.batteryVolt = 0;
}
#endif

#if FW_SINCE(0x00090804)
/* the next update after ticks */
void chargeTimerStart(unsigned int ticks)
{
	clr_flg(4, ~0x0020);
	timerStart(TIMER_RTC(0x17), ticks);
}

#if CHARGE_0104
/* DOL-1002's: off, or (on) the update restarted after its period and run */
void chargeTimerSet(unsigned char on)
{
	if (on) {
		timerStart(TIMER_RTC(0x17), charge.acDelay);
		task5SetFlag(4, 0x20);
	} else {
		timerStop(TIMER_RTC(0x17));
		clr_flg(4, ~0x0020);
	}
}
#else
void chargeTimerStop(void)
{
#if SOURCE_0103
	/* the slim's stops the timer first */
	timerStop(TIMER_RTC(0x17));
	clr_flg(4, ~0x0020);
#else
	clr_flg(4, ~0x0020);
	timerStop(TIMER_RTC(0x17));
#endif
}
#endif
#endif

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
	if (CHARGE_FACTOR_STATUS3) {
		charge.acDelay = 10;
		return;
	}
	switch (stateGet() & 0x7fff) {
	case 0x0020:
		charge.acDelay = 10;
		break;
	case 0x0004:
	case 0x0008:
#if (BOARD_CHARGE_TIMER1F && !BOARD_DOL1001) || BOARD_IS(0x00411000)
		charge.acDelay = 60;
#else
		charge.acDelay = 10;
#endif
		break;
	}
}
#endif

#if BOARD_DOL1001
/* DOL-1001's USB sources: a charger (2 or 3); it has no multi-connector */
unsigned char chargeUsbPower(void)
{
	return cookieStatus2.bits.vbus && (powerStatus.bits.usb == 2 || powerStatus.bits.usb == 3);
}

unsigned char chargeUsbDongle(void)
{
	return 0;
}

unsigned char chargeUsbDebug(void)
{
	return 0;
}

#if CHARGE_0104
unsigned char chargeUsbMultiCn2(void)
{
	return 0;
}
#endif

/* the AC, or a USB charger (2 or 3) */
unsigned char chargeExternalPower(void)
{
	return powerStatus.bits.ac ||
	       (cookieStatus2.bits.vbus && (powerStatus.bits.usb == 2 || powerStatus.bits.usb == 3));
}
#elif SOURCE_0103
/* the slim's USB sources: a charger (2, 3, 5 or 6) or the multi-connector's level 2 */
unsigned char chargeUsbPower(void)
{
	return cookieStatus2.bits.vbus &&
	       (powerStatus.bits.usb == 2 || powerStatus.bits.usb == 3 || powerStatus.bits.usb == 5 ||
#if BOARD_MULTICN_LEVELS
		powerStatus.bits.usb == 6 || chargeUsbMultiCn2() || chargeUsbDongle());
#else
		powerStatus.bits.usb == 6 || chargeUsbMultiCn2());
#endif
}

/* the USB connected, and the multi-connector's dongle, its debug board, its level 2 */
unsigned char chargeUsbDongle(void)
{
	if (chargeUsbState.0 && boardMultiCnIsDongle())
		return 1;
	return 0;
}

#if FW_SINCE(0x01030511)
unsigned char chargeUsbDebug(void)
{
	if (chargeUsbState.0 && boardMultiCnIsDebug())
		return 1;
	return 0;
}
#endif

unsigned char chargeUsbMultiCn2(void)
{
#if BOARD_MULTICN_LEVELS
	if (chargeUsbState.0 && boardMultiCnIsLevel2())
#else
	if (chargeUsbState.0 && analogGetMultiCn() == 2)
#endif
		return 1;
	return 0;
}

/* the AC, or a USB charger (2 or 3) */
unsigned char chargeExternalPower(void)
{
	return powerStatus.bits.ac ||
	       (cookieStatus2.bits.vbus && (powerStatus.bits.usb == 2 || powerStatus.bits.usb == 3));
}
#endif

#if FW_SINCE(0x01000109)
#if BOARD_DOL1002
/* a USB host: status 1 */
unsigned char chargeUsbIs1(void)
{
	return cookieStatus2.bits.vbus && powerStatus.bits.usb == 1;
}
#elif BOARD_DOL1001
/* a USB host: status 1 with no dongle or debug board */
unsigned char chargeUsbIs1(void)
{
	return cookieStatus2.bits.vbus && powerStatus.bits.usb == 1 && !chargeUsbDongle() &&
	       !chargeUsbDebug();
}
#elif SOURCE_0103
/* a USB host: status 1 with none of the multi-connector's, and its level read */
unsigned char chargeUsbIs1(void)
{
	return cookieStatus2.bits.vbus && powerStatus.bits.usb == 1 && !chargeUsbDongle() &&
#if FW_SINCE(0x01030511)
	       !chargeUsbDebug() &&
#endif
#if BOARD_MULTICN_LEVELS
	       !chargeUsbMultiCn2() && !boardMultiCnIsUnread();
#else
	       !chargeUsbMultiCn2() && analogGetMultiCn() != 0xfe;
#endif
}
#else
int chargeUsbIs1(void)
{
	return powerStatus.bits.usb == 1;
}
#endif

#if SOURCE_0103
unsigned char chargeUsbIs2(void)
#else
int chargeUsbIs2(void)
#endif
{
	return cookieStatus2.bits.vbus && powerStatus.bits.usb == 2;
}
#endif

#if SOURCE_0103
/* the USB connected, with VBUS (input.c) */
unsigned char chargeVbusActive(void)
{
	return chargeUsbState.0 && cookieStatus2.bits.vbus;
}

/* the manual charge mode on or off (0x08c0): its settings sent, or the charge's again */
unsigned char chargeSetManualMode(unsigned char on)
{
	unsigned char result;

	result = 0;
	if (on == 1) {
		if (!configGet()->charge.bits.manualOffCold ||
		    (!(1 & charge.alarmsOn.byte) && !powerStatus.bits.cookieLock0)) {
			result = chargeManualOn();
			if (result == 0) {
				chargeManualMode = 1;
			} else {
				chargeManualMode = 0xff;
				chargeManualOff();
			}
		} else {
			/* not while the cold range or alarm 0 stops the charge */
			result = 0xff;
		}
	} else if (chargeManualMode != 0) {
		chargeManualMode = 0;
		result = chargeManualOff();
		if (result != 0)
			chargeManualMode = 0xff;
	}
	return result;
}

#if CHARGE_0104
/* DOL-1002's serial operation: a new mode drops the last status registers and has task 5 read
   the charge again (event 0x20); 0 */
unsigned char chargeSetSerialMode(unsigned char mode)
{
	if (chargeSerialMode != mode) {
		chargeSerialMode = mode;
		chargeStatus1Last.word = 0;
		chargeStatus2Last.word = 0;
		task5SetFlag(4, 0x20);
	}
	return 0;
}
#endif

/* the manual mode's settings */
unsigned char chargeManualOn(void)
{
	unsigned char result;

	result = 0;
	result = cookieCtrlSupplyCurrentUsb(configGet()->manualSupplyCurrentUsb);
	if (result != 0)
		return result;
	result = cookieCtrlSupplyCurrent(configGet()->manualSupplyCurrent);
	if (result != 0)
		return result;
	result = cookieCtrlChargingCurrent(configGet()->manualChargingCurrent);
	if (result != 0)
		return result;
	result = cookieCtrlReg8(configGet()->manualReg8);
	if (result != 0)
		return result;
	result = cookieCtrlReg6Bit3(1);
	if (result != 0)
		return result;
	result = cookieSetChargeAc(1);
	if (result != 0)
		return result;
	result = cookieCtrlBatteryCharge(1);
	if (result != 0)
		return result;
	return result;
}

/* and the charge's again: the last error answered */
unsigned char chargeManualOff(void)
{
	unsigned char result;
	unsigned char r;

	result = 0;
	r = chargeSetCurrent();
	if (r != 0)
		result = r;
	r = cookieUpdateSupplyCurrent(chargeCookieSetting);
	if (r != 0)
		result = r;
	if (chargeTemp.setting1 == 0xff) {
		r = cookieCtrlCharge(0);
		if (r != 0)
			result = r;
	} else {
		r = cookieCtrlCharge(1);
		if (r != 0)
			result = r;
		r = cookieUpdateChargingCurrent(chargeTemp.setting1);
		if (r != 0)
			result = r;
	}
	if (chargeTemp.setting2 != 0xff) {
		r = cookieUpdateReg8(chargeTemp.setting2);
		if (r != 0)
			result = r;
	}
	r = cookieUpdateBatteryCharge(1);
	if (r != 0)
		result = r;
	chargeSetAcDelay();
	task5SetFlag(4, 0x20);
	return result;
}

unsigned char chargeGetManualMode(void)
{
	return chargeManualMode;
}

#if BOARD_DOL1001
/* DOL-1001 has no battery: the temperature sensor's reading (thermalPoll, in 0.1 K) takes the
   battery's place in the charge alarms, at most each chargeTempInterval RTC ticks */
void chargeSetTemp(unsigned int temperature)
{
	unsigned int ticks;

	timerGetRtcTicks(&ticks);
	if (ticks - charge.checkTicks > configGet()->chargeTempInterval) {
		charge.thermalTemp = temperature;
		charge.checkTicks = ticks;
		chargeAlarmUpdate();
	}
}
#endif

/* the USB's modes 1-3 to 1 or 4 by the battery's level 9 */
void chargeUsbModeUpdate(void)
{
	switch (chargeUsbMode) {
	case 0:
		break;
	case 1:
	case 2:
	case 3:
		if (chargeBatteryLevel9())
			chargeUsbMode = 1;
		else
			chargeUsbMode = 4;
		chargeSetCurrent();
		break;
	case 4:
	case 5:
		break;
	}
}

#if CHARGE_0104
/* a supply current at most chargeCookieSetting and chargeTempSetting */
unsigned char chargeLimitCurrent(unsigned char current)
{
	if (chargeCookieSetting < current)
		current = chargeCookieSetting;
	if (chargeTempSetting < current)
		current = chargeTempSetting;
	return current;
}
#endif
#endif
