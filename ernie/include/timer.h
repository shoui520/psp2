/*
 * The software timers (app/timer.c): a timer runs its handler when its count of ticks runs
 * out.
 */
#ifndef ERNIE_TIMER_H
#define ERNIE_TIMER_H

#include "ernie.h"

struct TimerHandler {
	void		(*handler)(void);
};

/* a timer: armed (1) until its count of ticks reaches time, then its handler runs */
struct Timer {
	unsigned char	state;
	unsigned int	time;
	void		(*handler)(void);
};

/* timers 0x00-0x14 count timerTicks, 0x15-0x1f timerRtcTicks (the RTC's, every 0.5 s); the
   updater's 0x15-0x1d count the clock. Before 0x01000306 the application has no timer 0x14
   (the power LED's fade step): its RTC timers are 0x14-0x1e, each one below its number since,
   which TIMER_RTC() gives */
#if defined(ERNIE_UPDATER) || FW_SINCE(0x01000306)
#define TIMER_RTC_FIRST		0x15
#define TIMER_RTC(id)		(id)
#else
#define TIMER_RTC_FIRST		0x14
#define TIMER_RTC(id)		((id) - 1)
#endif
#ifdef ERNIE_UPDATER
#define TIMER_COUNT		0x1e
#else
#define TIMER_COUNT		(TIMER_RTC(0x1f) + 1)
#endif

extern __near struct Timer timers[TIMER_COUNT];
extern __sreg unsigned int timerTicks;
extern __sreg unsigned int timerRtcTicks;

extern int timerInit(void);
extern int timerSetHandler(unsigned char id, struct TimerHandler *handler);
extern int timerStart(unsigned char id, unsigned int ticks);
extern int timerStop(unsigned char id);
extern int timerGetTicks(unsigned int *ticks);
extern int timerSetTicks(unsigned int *ticks);
extern int timerArmed(void);
extern int timerSetRtcTicks(unsigned int *ticks);
extern int timerGetRtcTicks(unsigned int *ticks);
extern void timerTick(void);

#endif
