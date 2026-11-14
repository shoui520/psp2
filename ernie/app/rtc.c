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
#include "sysinfo.h"
#include "task.h"
#include "tau.h"
#include "timer.h"

#if FW_SINCE(0x01000104)
/* the alarm's mode, the fifth byte of commands 0x0012 and 0x0082 */
unsigned char alarmMode;
#endif

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
#if FW_SINCE(0x01000306)
	button2HoldReset();
#endif
	RTCIF = 0;
	RTCMK = 0;
#if !FW_SINCE(0x01000104)
	/* before 0x01000104 a warm reset keeps the clock and the alarm, which has no mode */
	switch (getResetCause()) {
	case 0xf1:
	case 0x10:
	case 0x11:
		break;
	default:
		setClock(0);
		setAlarm(RTC_ALARM_NONE);
		sleepFactorSet3(0);
		break;
	}
#else
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
#endif
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

unsigned int sleepFactorBit3Get(void)
{
	return sleepFactorGet() & 8;
}

#if FW_SINCE(0x01000104)
void setAlarmMode(unsigned char mode)
{
	alarmMode = mode;
}

unsigned char getAlarmMode(void)
{
	return alarmMode;
}
#endif

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
#if FW_SINCE(0x01000107)
	timerRtcTicks++;
#endif
	EI();
	rtcClockLow = keepRam.clock;
	if (state == 0x0010) {
		levelMonitor1Poll();
		input1Bit7Poll();
	} else if (state == 0x0800)
		levelMonitor1Poll();
	if (rtcClockLow & 0x0001) {
		task2SetFlag(3, 0x8000);
#if FW_SINCE(0x01000107)
		if (hangDetected() != 0) {
			hangCount++;
			hangState = HANG_COUNTING;
			if (hangCount >= 8 && hangLimitReached() != 0) {
				DI();
				hangState = HANG_RESET;
				setResetCause(0x30);
				resetSystem();
			}
		} else {
			hangCount = 0;
			hangState = HANG_NONE;
		}
#endif
	}
#if FW_SINCE(0x01000306)
	if (!P4.6) {
		if (button2Keys() == 1) {
			holdTicks++;
			if (button2HoldReached(holdTicks) != 0)
				holdReached = 1;
		}
	} else {
		holdTicks = 0;
		holdReached = 0;
	}
#endif
	for (i = TIMER_RTC_FIRST; i < TIMER_COUNT; i++) {
#if FW_SINCE(0x01000107)
		if (timers[i].state == 1 && timers[i].time == timerRtcTicks) {
#else
		if (timers[i].state == 1 && timers[i].time == rtcClockLow) {
#endif
			timers[i].state = 0;
			timers[i].handler();
		}
	}
#if !FW_SINCE(0x01000107)
	/* before 0x01000107 a hang starts timer 11, whose interrupt counts it (tm11Interrupt) */
#if FW_SINCE(0x01000104)
	if (hangDetected() != 0 && (hangState != HANG_COUNTING || !hangTimerRunning())) {
#else
	if (HANG_DETECTED() && hangState != HANG_COUNTING) {
#endif
		hangState = HANG_COUNTING;
		hangCount = 0;
		tau1PowerOn();
		tm11Stop();
		tm11Start();
	}
#endif
}
