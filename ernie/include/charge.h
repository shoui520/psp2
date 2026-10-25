/*
 * The charge (app/charge.c): the Cookie's status registers 1 and 2, the power status (host
 * command 0x0800) made from them, the battery's readings and the charge's sleep factors.
 */
#ifndef ERNIE_CHARGE_H
#define ERNIE_CHARGE_H

/* the Cookie's status register 1 (cookieReadStatus1), which the power status copies
   (chargeSetPowerStatus) */
union CookieStatus1 {
	unsigned int	word;
	unsigned char	byte[2];
	struct {
		unsigned char	ac:1;		/* the power status's bit 5 */
		unsigned char	battery:1;	/* bit 6: 0x40 a Vita's, 0x60 a PS TV's */
		unsigned char	charging:1;	/* bit 7 */
		unsigned char	level:2;	/* the power status's bits 14-15 */
		unsigned char	state:3;	/* 4: powerCookieHold holds */
		unsigned char	reserved:4;
		unsigned char	status1:1;	/* the power status's bit 1 */
		unsigned char	status2:1;	/* bit 2 */
		unsigned char	status3:2;	/* bits 3-4 */
	} bits;
};

/* register 2 (cookieReadStatus2) */
union CookieStatus2 {
	unsigned int	word;
	unsigned char	byte[2];
	struct {
		unsigned char	reserved1;
		unsigned char	reserved2:2;
		unsigned char	usb:2;		/* the power status's bits 8-9 (0x089a) */
		unsigned char	reserved3:2;
		unsigned char	vbus:1;
		unsigned char	vbusCharge:1;	/* set: chargeSetCurrent at each update */
	} bits;
};

/* a battery temperature range, while the temperature is below its limit (limit[1] when it
   fell or held since the last change of range) */
struct ChargeTempLevel {
	unsigned int	limit[2];
	unsigned char	setting1;		/* cookieCtrlChargingCurrent's; 0xff: no charge */
	unsigned char	setting2;		/* cookieCtrlReg8's; 0xff: left as it is */
};

/* the battery temperature's range (chargeTempUpdate) */
struct ChargeTemp {
	unsigned char	setting1;		/* the settings in force */
	unsigned char	setting2;
	unsigned char	level;			/* the range in force */
	unsigned char	read;			/* this read's range */
	unsigned char	readLast;		/* and the last read's: a range is taken when two
						   reads agree */
	unsigned int	temp;			/* the temperature at the last change of range */
};

/* a battery temperature alarm's limits: on at high or above, off at low or below, each held
   for its time before the alarm's handler runs */
struct ChargeAlarmLimit {
	unsigned int	high;
	unsigned int	low;
	unsigned int	holdOn;			/* in RTC ticks */
	unsigned int	holdOff;
	unsigned char	bicTemp;		/* set: the BIC's temperature, else the battery's */
};

union ChargeAlarmFlags {
	unsigned char	byte;
	struct {
		unsigned char	on:1;		/* the alarm as its handler last saw it */
		unsigned char	over:1;		/* the temperature past the limits */
		unsigned char	overLast:1;	/* and at the last check */
		unsigned char	reserved:5;
	} bits;
};

/* a battery temperature alarm (chargeAlarmUpdate) */
struct ChargeAlarm {
	unsigned int	since;			/* the ticks when over last changed */
	union ChargeAlarmFlags flags;
	void		(*handler)(void);
};

/* the alarms on, as their handlers set them */
union ChargeAlarmsOn {
	unsigned char	byte;
	struct {
		unsigned char	alarm0:1;
		unsigned char	alarm1:1;
		unsigned char	alarm2:1;
		unsigned char	reserved:5;
	} bits;
};

/* a battery level: at or below it when the remaining capacity or the voltage is (both, with
   bit 15 of remainCap) */
struct ChargeLevel {
	unsigned int	remainCap;
	unsigned int	volt;
};

struct Charge {
	unsigned char	timer1fPending;		/* cleared when timer 0x1f runs */
	unsigned int	flags;			/* the battery's levels (chargeReadFlags), each
						   taken when two reads agree; 15: no battery */
	unsigned int	flagsRead;		/* this update's read */
	unsigned int	flagsLast;		/* and the last one's */
	unsigned int	batteryRemainCap;	/* the battery's readings: with no Cookie the */
	unsigned int	batteryVolt;		/* settings' */
	unsigned int	batteryTemp;		/* in 0.1 K */
	unsigned int	batteryBicTemp;
	unsigned int	batteryLifePercent;
	unsigned int	batteryTempC;		/* in 0.1 degrees C */
	unsigned int	batteryBicTempC;
	unsigned int	acDelay;		/* timer 0x17's, the update's period */
	unsigned int	tempTicks;		/* the RTC's ticks at the last update past 0x79 */
	unsigned int	checkTicks;		/* and at the last chargeAlarmUpdate */
	union ChargeAlarmsOn alarmsOn;
	struct ChargeAlarm alarms[4];
};

extern __sreg union CookieStatus1 cookieStatus1;
extern __sreg union CookieStatus2 cookieStatus2;
extern __sreg unsigned char chargeFactors;	/* the charge's sleep factors: 0 status 3 (the AC or
						   the USB status's bit 1), 1 status 4, 2-5 16-19,
						   6 22, 7 23 */
extern __sreg unsigned char chargeControl;	/* 0: transition 0x16 due at the AC (chargeUpdate);
						   1: the AC's charge on (0x0892) */
extern __sreg struct ChargeTemp chargeTemp;
extern __sreg unsigned char chargeCurrent;	/* the charge setting sent (cookieCtrlSupplyCurrentUsb); 0xff none */
extern __sreg unsigned char chargeUsbMode;	/* the USB status's mode (0x089a) */
extern __sreg unsigned char chargeUsbHold;	/* 0xff: mode 5 while charging; no transition 0x0f */
extern __sreg unsigned char chargeVbus;		/* the VBUS charge's bits (0x0893) */
extern __sreg unsigned char chargeBbmcVersion;	/* the 3G modem's version at the last update */
extern __sreg unsigned char chargeCookieSetting;	/* cookieCtrlSupplyCurrentAc's (alarm 2) */
extern __sreg unsigned char chargeCookieHeld;	/* powerCookieHold's */
extern struct Charge charge;

extern void chargeInit(void);
extern void chargeTransition(int transition);
extern void chargeUpdate(void);
extern void chargeSetPowerStatus(void);
extern unsigned int chargeReadFlags(void);
extern void chargeTempUpdate(unsigned int temp, struct ChargeTemp *t);
extern void chargeAlarmUpdate(void);
extern void chargeAlarmInit(void);
extern void chargeTimer(void);
extern void chargeCookieTimer(void);
extern void chargeTimer1f(void);
extern void chargeUpdateSoon(void);
extern int chargeSetCurrent(void);
extern int chargeMayStart(void);
extern int chargeAlarm0On(void);
extern char chargeBatteryLevel9(void);
extern char chargeBatteryLevel0(void);
extern char chargeBatteryLevel1(void);
extern void chargeSetBattery(void);
extern int chargeBbmcChanged(void);
extern void chargeResetBattery(void);
extern void chargeTimerStart(unsigned int ticks);
extern void chargeTimerStop(void);
extern unsigned char chargeGetAc(void);
extern unsigned char chargeGetCharging(void);
extern void chargeReadTempVolt(unsigned int *value);
extern void chargeUnused0(void);
extern void chargeUnused1(void);
extern void chargeSetAcDelay(void);
extern int chargeUsbIs1(void);
extern int chargeUsbIs2(void);

#endif
