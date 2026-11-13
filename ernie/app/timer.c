/*
 * The software timers: cyclic handler 7 counts timerTicks and runs timers 0x00-0x14 whose
 * time came; the RTC's interrupt does the same for timers 0x15-0x1f with timerRtcTicks.
 */
#pragma DI
#pragma EI

#include <stddef.h>

#include "ernie.h"
#include "kernel.h"
#include "rtc.h"
#include "timer.h"

__near struct Timer timers[TIMER_COUNT];
__sreg unsigned int timerTicks;
#if FW_SINCE(0x01000107)
__sreg unsigned int timerRtcTicks;
#endif

int timerInit(void)
{
	unsigned char i;
	unsigned int ticks;

	for (i = 0; i < TIMER_COUNT; i++) {
		timers[i].time = 0;
		timers[i].state = 0;
		timers[i].handler = NULL;
	}
	ticks = 0;
	timerSetTicks(&ticks);
#if FW_SINCE(0x01000107)
	timerSetRtcTicks(&ticks);
#endif
	sta_cyc(7);
	return 0;
}

int timerSetHandler(unsigned char id, struct TimerHandler *handler)
{
	if (id < TIMER_COUNT) {
		timers[id].handler = handler->handler;
		return 0;
	}
	return 1;
}

/* arms a timer to run its handler after ticks more ticks */
int timerStart(unsigned char id, unsigned int ticks)
{
	if (id < TIMER_RTC_FIRST) {
		DI();
		timers[id].time = ticks + timerTicks + 1;
		timers[id].state = 1;
		EI();
		return 0;
	}
	if (id < TIMER_COUNT) {
		DI();
#if FW_SINCE(0x01000107)
		timers[id].time = ticks + timerRtcTicks + 1;
#else
		/* before 0x01000107 the RTC timers count the clock's low word */
		timers[id].time = (unsigned int)getClock().value + ticks + 1;
#endif
		timers[id].state = 1;
		EI();
		return 0;
	}
	return 1;
}

int timerStop(unsigned char id)
{
	if (id < TIMER_COUNT) {
		DI();
		timers[id].state = 0;
		timers[id].time = 0;
		EI();
		return 0;
	}
	return 1;
}

int timerGetTicks(unsigned int *ticks)
{
	DI();
	*ticks = timerTicks;
	EI();
	return 0;
}

int timerSetTicks(unsigned int *ticks)
{
	DI();
	timerTicks = *ticks;
	EI();
	return 0;
}

/* 1 while any of timers 0x00-0x14 is armed */
int timerArmed(void)
{
	int result;
	unsigned char i;

	result = 0;
	for (i = 0; i < TIMER_RTC_FIRST; i++) {
		if (timers[i].state == 1) {
			result = 1;
			break;
		}
	}
	return result;
}

#if FW_SINCE(0x01000107)
int timerSetRtcTicks(unsigned int *ticks)
{
	DI();
	timerRtcTicks = *ticks;
	EI();
	return 0;
}

int timerGetRtcTicks(unsigned int *ticks)
{
	DI();
	*ticks = timerRtcTicks;
	EI();
	return 0;
}
#else
int timerGetRtcTicks(unsigned int *ticks)
{
	DI();
	*ticks = (unsigned int)getClock().value;
	EI();
	return 0;
}
#endif

/* cyclic handler 7: a tick */
void timerTick(void)
{
	unsigned char i;

	DI();
	timerTicks++;
	for (i = 0; i < TIMER_RTC_FIRST; i++) {
		if (timers[i].state == 1 && timers[i].time == timerTicks) {
			timers[i].state = 0;
			timers[i].handler();
		}
	}
	EI();
}
