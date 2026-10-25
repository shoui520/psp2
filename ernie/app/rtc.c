/*
 * The real-time clock: the RTC's counters and alarm cleared and the clock started, and the
 * clock and the alarm kept in the RAM kept over a reset, read and written with interrupts
 * disabled (the RTC's interrupt counts the clock).
 */
#pragma sfr
#pragma DI
#pragma EI

#include "ernie.h"
#include "keepram.h"
#include "rtc.h"

extern void sub_1278E(void);
extern unsigned char getResetCause(void);
extern void sub_14D53(int);
extern unsigned long sub_14CCA(void);

/* the alarm's mode, the fifth byte of commands 0x0012 and 0x0082 */
unsigned char alarmMode;

/* the counters at 0 (1 January), the 24-hour clock, an interrupt every 0.5 s */
void rtcInit(void)
{
	RTCEN = 1;
	RTCE = 0;
	RTCC0 = 0x09;
	RTCC1 = 0x00;
	SEC = 0;
	MIN = 0;
	HOUR = 0;
	DAY = 1;
	WEEK = 0;
	MONTH = 1;
	YEAR = 0;
	SUBCUD = 0;
	ALARMWM = 0;
	ALARMWH = 0;
	ALARMWW = 0;
}

void rtcStart(void)
{
	RTCMK = 1;
	rtcInit();
	sub_1278E();
	RTCIF = 0;
	RTCMK = 0;
	switch (getResetCause()) {
	case 0xf1:
	case 0x10:
	case 0x11:
		setAlarm(RTC_ALARM_NONE);
		sub_14D53(0);
		setAlarmMode(0);
		break;
	case 0x40:
		break;
	default:
		setClock(0);
		setAlarm(RTC_ALARM_NONE);
		sub_14D53(0);
		setAlarmMode(0);
		break;
	}
	RTCE = 1;
}

union RtcTick getClock(void)
{
	union RtcTick tick;

	DI();
	tick.value = keepRam.clock;
	EI();
	return tick;
}

void setClock(unsigned long clock)
{
	DI();
	keepRam.clock = clock;
	EI();
	return;
}

union RtcTick getAlarm(void)
{
	union RtcTick tick;

	DI();
	tick.value = keepRam.alarm;
	EI();
	return tick;
}

void setAlarm(unsigned long alarm)
{
	DI();
	keepRam.alarm = alarm;
	EI();
	return;
}

int isAlarmReached(void)
{
	int reached;

	DI();
	reached = keepRam.clock >= keepRam.alarm;
	EI();
	return reached;
}

unsigned int sub_11118(void)
{
	return sub_14CCA() & 8;
}

void setAlarmMode(unsigned char mode)
{
	alarmMode = mode;
}

unsigned char getAlarmMode(void)
{
	return alarmMode;
}
