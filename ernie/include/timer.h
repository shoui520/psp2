/*
 * The software timers (the module at 0x2407a, not written yet): a timer runs its handler when
 * its count of ticks runs out.
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

extern int sub_240CF(unsigned char id, struct TimerHandler *handler);
extern void sub_24110(int id, unsigned int ticks);
extern void sub_24163(int id);

#endif
