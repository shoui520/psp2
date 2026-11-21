/*
 * The timer array units: TAU0 and TAU1 powered on and off, and the interval timer on
 * channel 0 of TAU0.
 */
#pragma sfr

#include "ernie.h"
#include "tau.h"

void tau0PowerOn(void)
{
	if (!TAU0EN) {
		TAU0EN = 1;
#if CHIP_RL78
		TPS0 = 0x80;	/* the RL78's 16-bit register */
#else
		TPS0L = 0x80;
#endif
	}
}

void tau0PowerOff(void)
{
	TAU0EN = 0;
}

#if !SOURCE_0103	/* the PS TV's has no TAU1 */
void tau1PowerOn(void)
{
	if (!TAU1EN) {
		TAU1EN = 1;
#if CHIP_RL78
		TPS1 = 0x80;	/* the RL78's 16-bit register */
#else
		TPS1L = 0x80;
#endif
	}
}

void tau1PowerOff(void)
{
	TAU1EN = 0;
}
#endif

void tm00Start(void)
{
	TMR00 = 0x0000;
	TDR00 = 0x7d00;
	TMIF00 = 0;
	TMMK00 = 0;
	TS0L.0 = 1;
}

void tm00Stop(void)
{
	TMMK00 = 1;
	TMIF00 = 0;
	TT0L.0 = 1;
}
