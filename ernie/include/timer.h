/*
 * The software timers (the module at 0x2407a, not written yet): a timer runs its handler when
 * its count of ticks runs out.
 */
#ifndef ERNIE_TIMER_H
#define ERNIE_TIMER_H

struct TimerHandler {
	void		(*handler)(void);
};

extern int sub_240CF(unsigned char id, struct TimerHandler *handler);
extern void sub_24110(int id, unsigned int ticks);
extern void sub_24163(int id);

#endif
