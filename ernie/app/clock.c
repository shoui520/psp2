/*
 * USS-1001's clocks: every peripheral off, the X1 oscillator started and selected as the main
 * clock, the internal oscillator at its divider 2, then a 0x280-count wait on TM00. P4.0
 * (debugpin.c's) marks the steps for a probe.
 */
#pragma sfr
#pragma NOP

#include "ernie.h"
#include "clock.h"
#include "tau.h"

void clockInit(void)
{
	PER0 = 0x00;
	PER1 = 0x00;
	CMC = 0x72;
	CSC = 0xc0;
	CKC = 0x00;
	OSMC = 0x00;
	HOCODIV = 0x02;
	P4.0 = 0;
	PM4.0 = 0;
	OSTS = 0x01;
	MSTOP = 0;
	P4.0 = 1;
	while (OSTC != 0xc0)
		;
	P4.0 = 0;
	CKC = 0x10;
	while (!MCS)
		;
	P4.0 = 1;
	XTSTOP = 0;
	tau0PowerOn();
	TMR00 = 0x8000;
	TDR00 = 0x0280;
	TMIF00 = 0;
	TMMK00 = 1;
	TS0L.0 = 1;
	while (!TMIF00)
		NOP();
	tm00Stop();
	tau0PowerOff();
	P4.0 = 0;
}
