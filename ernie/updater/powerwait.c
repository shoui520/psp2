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
	TS0L = 0x08;
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
	TMMK03 = 1;
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
	unsigned char last;
	unsigned char level;

	last = 0;
	level = 0;
	while (!(level & last)) {
		powerWaitMs(8);
		last = level;
		level = ~P12.6 & 1;
	}
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
