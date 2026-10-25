/*
 * The software timers (app/timer.c): a timer runs its handler when its count of ticks runs
 * out.
 */
#ifndef ERNIE_TIMER_H
#define ERNIE_TIMER_H

struct TimerHandler {
	void		(*handler)(void);
};

/* a timer: armed (1) until its count of ticks reaches time, then its handler runs */
struct Timer {
	unsigned char	state;
	unsigned int	time;
	void		(*handler)(void);
};

/* timers 0x00-0x14 count timerTicks, 0x15-0x1f timerRtcTicks (the RTC's, every 0.5 s) */
#define TIMER_RTC_FIRST		0x15
#define TIMER_COUNT		0x20

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
