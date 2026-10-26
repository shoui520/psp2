/*
 * The power sequences' waits (app/powerwait.c): the device jobs task 5 runs (powerDeviceSetPending posts
 * their bits), the waits on TM03 and the kernel's ticks, the wait for the power input's edge,
 * and the timers 4-0xe whose bits a sequence waits on.
 */
#ifndef ERNIE_POWERWAIT_H
#define ERNIE_POWERWAIT_H

#include "kernel.h"

/* a device job: run when its bit is posted */
struct PowerDeviceJob {
	unsigned int	bits;
	unsigned char	(*job)(void);
};

extern unsigned int powerTimers;	/* the bits of the timers running (powerStartTimer) */

extern void powerDeviceRun(void);
extern void powerDeviceInit(void);
extern void powerWaitInit(void);
extern void powerWaitMs(unsigned int ms);
extern void powerSleep(unsigned int ms);
extern void powerSleep2(unsigned int ms);
extern void powerWaitInput(unsigned char edges, unsigned int timeout);
extern void powerWaitTimers(unsigned int bits);
extern void powerStartTimer(unsigned int bits, unsigned char id, unsigned int ticks);
extern void powerTimersInit(void);

#endif
