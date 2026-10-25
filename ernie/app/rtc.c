/*
 * The real-time clock: the RTC's counters and alarm cleared and the clock started, and the
 * clock and the alarm kept in the RAM kept over a reset, read and written with interrupts
 * disabled (the RTC's interrupt counts the clock).
 */
#pragma rtos_interrupt INTRTC rtcInterrupt
#pragma sfr
#pragma DI
#pragma EI

#include "ernie.h"
#include "input.h"
#include "keepram.h"
#include "rtc.h"
#include "sleepfactor.h"
#include "state.h"
#include "task.h"
#include "timer.h"

extern int sub_2D6C6(void);
extern int sub_2D69C(void);

extern __near unsigned char byteFDD2C;
extern __near unsigned char byteFDD2D;

/* the alarm's mode, the fifth byte of commands 0x0012 and 0x0082 */
unsigned char alarmMode;

/* the clock's low word at the last interrupt: odd every other one, once a second */
static __sreg unsigned int rtcClockLow;

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
		sleepFactorSet3(0);
		setAlarmMode(0);
		break;
	case 0x40:
		break;
	default:
		setClock(0);
		setAlarm(RTC_ALARM_NONE);
		sleepFactorSet3(0);
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
	return sleepFactorGet() & 8;
}

void setAlarmMode(unsigned char mode)
{
	alarmMode = mode;
}

unsigned char getAlarmMode(void)
{
	return alarmMode;
}

/*
 * Every 0.5 s: the clock and the RTC timers' ticks counted, the input's hold checks, once a
 * second the check that resets the system after 8 failing seconds, then the RTC timers
 * whose time came.
 */
void rtcInterrupt(void)
{
	unsigned char i;
	unsigned int state;

	state = stateGet();
	DI();
	keepRam.clock++;
	timerRtcTicks++;
	EI();
	rtcClockLow = keepRam.clock;
	if (state == 0x0010) {
		sub_125F2();
		sub_1267E();
	} else if (state == 0x0800)
		sub_125F2();
	if (rtcClockLow & 0x0001) {
		task2SetFlag(3, 0x8000);
		if (sub_2D6C6() != 0) {
			byteFDD2D++;
			byteFDD2C = 1;
			if (byteFDD2D >= 8 && sub_2D69C() != 0) {
				DI();
				byteFDD2C = 2;
				setResetCause(0x30);
				resetSystem();
			}
		} else {
			byteFDD2D = 0;
			byteFDD2C = 0;
		}
	}
	if (!P4.6) {
		if (sub_12081() == 1) {
			holdTicks++;
			if (sub_12775(holdTicks) != 0)
				holdReached = 1;
		}
	} else {
		holdTicks = 0;
		holdReached = 0;
	}
	for (i = TIMER_RTC_FIRST; i < TIMER_COUNT; i++) {
		if (timers[i].state == 1 && timers[i].time == timerRtcTicks) {
			timers[i].state = 0;
			timers[i].handler();
		}
	}
}
