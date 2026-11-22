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
#if !defined(ERNIE_UPDATER) && FW_SINCE(0x0100060B)
		timers[i].handlerSet = 0;
#endif
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
#if !defined(ERNIE_UPDATER) && FW_SINCE(0x0100060B)
		timers[id].handlerSet = 1;
#endif
		return 0;
	}
	return 1;
}

/* arms a timer to run its handler after ticks more ticks */
#if !defined(ERNIE_UPDATER) && FW_SINCE(0x0100060B)
/* 0x0100060B: 1 for no such timer, 2 for one without a handler; the last test cannot fail,
   and its path returns nothing */
int timerStart(unsigned char id, unsigned int ticks)
{
	if (id >= TIMER_COUNT)
		return 1;
	if (timers[id].handlerSet == 0)
		return 2;
	if (id < TIMER_RTC_FIRST) {
		DI();
		timers[id].time = ticks + timerTicks + 1;
		timers[id].state = 1;
		EI();
		return 0;
	}
	if (id < TIMER_COUNT) {
		DI();
		timers[id].time = ticks + timerRtcTicks + 1;
		timers[id].state = 1;
		EI();
		return 0;
	}
}
#else
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

#endif
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
#if FW_SINCE(0x00090502)
	for (i = 0; i < TIMER_RTC_FIRST; i++) {
#else
	/* 0x00090206 runs every timer, the RTC's too, by these ticks */
	for (i = 0; i < TIMER_COUNT; i++) {
#endif
		if (timers[i].state == 1 && timers[i].time == timerTicks) {
			timers[i].state = 0;
#if !defined(ERNIE_UPDATER) && FW_SINCE(0x0100060B)
			if (timers[i].handler != NULL)
#endif
				timers[i].handler();
		}
	}
	EI();
}
