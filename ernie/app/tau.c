/*
 * The timer array units: TAU0 and TAU1 powered on and off, and the interval timers on
 * channels 0 of TAU0 and 0 and 1 of TAU1 started and stopped. TM10 is the tick.
 */
#pragma rtos_interrupt INTTM10 tickInterrupt
#pragma sfr

#include "ernie.h"
#include "kernel.h"

void tau1PowerOn(void);
void tau1PowerOff(void);
void tm10Start(void);
void tm10Stop(void);

void tickStart(void)
{
	tau1PowerOn();
	tm10Start();
	return;
}

void tickStop(void)
{
	tm10Stop();
	tau1PowerOff();
	return;
}

/* TM10's interrupt: a tick of the kernel's time */
void tickInterrupt(void)
{
	isig_tim();
}

void tau0PowerOn(void)
{
	if (!TAU0EN) {
		TAU0EN = 1;
		TPS0L = 0x80;
	}
}

void tau0PowerOff(void)
{
	TAU0EN = 0;
}

void tau1PowerOn(void)
{
	if (!TAU1EN) {
		TAU1EN = 1;
		TPS1L = 0xf0;
	}
}

void tau1PowerOff(void)
{
	TAU1EN = 0;
}

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

void tm10Start(void)
{
	TMR10 = 0x0000;
	TDR10 = 0x7d00;
	TMIF10 = 0;
	TMMK10 = 0;
	TS1L = 0x01;
}

void tm10Stop(void)
{
	TMMK10 = 1;
	TMIF10 = 0;
	TT1L.0 = 1;
}

void tm11Start(void)
{
	TMR11 = 0x8000;
	TDR11 = 0x00f4;
	TMIF11 = 0;
	TMMK11 = 0;
	TS1L = 0x02;
}

void tm11Stop(void)
{
	TMMK11 = 1;
	TMIF11 = 0;
	TT1L.1 = 1;
}
