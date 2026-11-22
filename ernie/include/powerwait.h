/*
 * The power sequences' waits (app/powerwait.c): the device jobs task 5 runs (powerDeviceSetPending posts
 * their bits), the waits on TM03 and the kernel's ticks, the wait for the power input's edge,
 * and the timers 4-0xe whose bits a sequence waits on.
 */
#ifndef ERNIE_POWERWAIT_H
#define ERNIE_POWERWAIT_H

#include "ernie.h"
#include "board.h"
#include "kernel.h"

/* a power timer's number 4-0xb: since 0x0100060B bits 0 and 3 (timers 4 and 7) have none, and
   the others are one or two earlier */
#if BOARD_USS1001 && SOURCE_0104
/* the slim's 0x0105 (its updater too): three earlier still (no timers 0-2) */
#define POWER_TIMER(id)		((id) < 7 ? (id) - 4 : (id) - 5)
#elif FW_SINCE(0x0100060B)
#define POWER_TIMER(id)		((id) < 7 ? (id) - 1 : (id) - 2)
#else
#define POWER_TIMER(id)		(id)
#endif

/* bit 11's timer: after 9 and 10's, in 0x00080504 the last power timer (no bits 12 and 13) */
#if FW_SINCE(0x00090006) || defined(ERNIE_UPDATER)
#define POWER_TIMER11		TIMER(0x0c)
#else
#define POWER_TIMER11		0x0e
#endif

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
#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
extern void powerWaitMsPolled(unsigned int ms);
#endif
extern void powerSleep(unsigned int ms);
extern void powerSleep2(unsigned int ms);
#if !(BOARD_USS1001 && SOURCE_0104)
extern void powerWaitInput(unsigned char edges, unsigned int timeout);
#endif
extern void powerWaitTimers(unsigned int bits);
extern void powerStartTimer(unsigned int bits, unsigned char id, unsigned int ticks);
extern void powerTimersInit(void);
#if BOARD_DOL1001
extern void intp9Enable(unsigned char enable);
extern void intp9Event(void);
#endif

#ifdef ERNIE_UPDATER
/* the updater's wait for the kernel's dly_tsk (updater/stub.c) */
extern void powerWaitLong(unsigned int ms);
#endif

#endif
