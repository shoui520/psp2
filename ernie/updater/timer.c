/*
 * The updater's software timers: the application's, polled from the main loop, with 30
 * timers and the clock (getClock) in place of timerRtcTicks.
 */
#pragma DI
#pragma EI

#include <stddef.h>

#include "ernie.h"
#include "rtc.h"
#include "timer.h"

__near struct Timer timers[TIMER_COUNT];
__sreg unsigned int timerTicks;

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
		timers[id].time = (unsigned int)getClock().value + ticks + 1;
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

/* the interrupts left disabled */
int timerSetTicks(unsigned int *ticks)
{
	DI();
	timerTicks = *ticks;
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

int timerGetRtcTicks(unsigned int *ticks)
{
	DI();
	*ticks = (unsigned int)getClock().value;
	EI();
	return 0;
}

/* a tick, from the main loop */
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
