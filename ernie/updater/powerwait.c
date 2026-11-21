/*
 * The power sequences' waits, as the updater has them: each on TM03 (31.25 kHz), polled; the
 * sleeps, the timers and the wait for the power input are TM03 waits too.
 */
#pragma sfr
#pragma NOP

#include "ernie.h"
#include "updater.h"

static unsigned int powerWaitReserved[2];
unsigned int powerTimers;

void powerWaitInit(void)
{
#if SOURCE_0103
	/* the PS TV's: started only when it is not running */
	if (!(0x08 & TE0L))
		TS0L = 0x08;
#else
	TS0L = 0x08;
#endif
	TMIF03 = 0;
}

void powerWaitMs(unsigned int ms)
{
	powerWaitInit();
	TDR03 = (unsigned long)ms * 1000 / 32 + (ms * 1000 % 32 ? 1 : 0);
	TS0L = 0x08;
	while (!TMIF03)
		NOP();
	TT0L = 0x08;
#if !SOURCE_0103
	TMMK03 = 1;
#endif
}

void powerSleep(unsigned int ms)
{
	powerWaitMs(ms);
}

void powerSleep2(unsigned int ms)
{
	powerWaitMs(ms);
}

/* until the power input (P12.6) is low in two samples 8 ms apart */
void powerWaitInput(unsigned char edges, unsigned int timeout)
{
#if !SOURCE_0103	/* the PS TV's: nothing */
	unsigned char last;
	unsigned char level;

	last = 0;
	level = 0;
	while (!(level & last)) {
		powerWaitMs(8);
		last = level;
		level = ~P12.6 & 1;
	}
#endif
}

/* until the timers of the bits ran out */
void powerWaitTimers(unsigned int bits)
{
	while (bits & powerTimers)
		powerWaitMs(10);
}

/* the timer's wait, at once */
void powerStartTimer(unsigned int bits, unsigned char id, unsigned int ticks)
{
	powerWaitMs(ticks);
}
