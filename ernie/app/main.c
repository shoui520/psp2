/*
 * The application's start: the clocks and interrupt priorities set before the C runtime
 * starts (hdwinit, which cstart calls), main, and the resets, which record their cause in
 * the RAM kept over a reset.
 */
#pragma sfr
#pragma DI

#include "ernie.h"
#include "board.h"
#if SOURCE_0103
#include "clock.h"
#endif
#include "clockout.h"
#include "host.h"
#include "keepram.h"
#include "rtc.h"
#include "tau.h"

#ifndef ERNIE_UPDATER
void intInit(void);

/* at 0xfd900 (app.lk), where the updater keeps it too */
struct KeepRam keepRam;

/* the flash library's data buffer (FSL_Init's), after the kernel's RAM (app.lk) */
__near unsigned char fslDataBuffer[0x100];

/* called by the kernel once it has made its objects, before the first task runs */
void kernelInitHook(void)
{
}

#endif /* !ERNIE_UPDATER */

#if BOARD_DOL1001
/* DOL-1001's clocks, set apart from hdwinit: the peripherals' clocks off, then the fat
   models' X1 oscillator as the main clock, P4.0 pulsed round the internal oscillator's stop */
void clockInit(void)
{
	PER0 = 0;
	PER1 = 0;
	CMC = 0x53;
	CSC = 0xc0;
	CKC = 0x09;
	OSMC = 0x01;
	P4.0 = 0;
	PM4.0 = 0;
	OSTS = 0x01;
	MSTOP = 0;
	P4.0 = 1;
	do {
	} while (OSTC != 0xc0);
	P4.0 = 0;
	CKC = 0x18;
	while (!MCS)
		;
	HIOSTOP = 1;
	P4.0 = 1;
	XTSTOP = 0;
	P4.0 = 0;
}

/* clockout.c's (the clock on P14.0), in main.c's object on DOL-1001 */
void clockOutputStart(void)
{
	PCLOE0 = 0;
	CKS0 = 0x08;
	PM14.0 = 0;
	PCLOE0 = 1;
}

void clockOutputStop(void)
{
	PCLOE0 = 0;
	PM14.0 = 1;
}
#endif

/* the PS TV's updater (updater.lk) links the clocks above and nothing else of this file */
#ifndef ERNIE_UPDATER

/* the clocks: the X1 oscillator as the main clock, the internal oscillator stopped */
void hdwinit(void)
{
#if SOURCE_0103	/* the slim's clocks (clock.c), DOL-1001's (above) */
	clockInit();
#else
	CMC = 0x53;
	CSC = 0xc0;
	CKC = 0x09;
	OSMC = 0x01;
#if FW_SINCE(0x0100060B)
	P4.0 = 0;
	PM4.0 = 0;
#endif
	OSTS = 0x01;
	MSTOP = 0;
#if FW_SINCE(0x0100060B)
	P4.0 = 1;
#endif
	do {
	} while (OSTC != 0xc0);
#if FW_SINCE(0x0100060B)
	P4.0 = 0;
#endif
	CKC = 0x18;
	while (!MCS)
		;
	HIOSTOP = 1;
	XTSTOP = 0;
#endif	/* SOURCE_0103 */
#if FW_SINCE(0x00090804)
	boardPortInit();
#else
	boardInit();
#endif
	intInit();
	tickStart();
	rtcInit();
	clockOutputStop();
	return;
}

void main(void)
{
	hostRequestInit();
	return;
}

/* every interrupt masked and at the lowest priority, the UARTs' receptions at level 2 */
void intInit(void)
{
	IF0 = 0;
	IF1 = 0;
	IF2 = 0;
	MK0 = 0xffff;
	MK1 = 0xffff;
	MK2 = 0xffff;
	PR00 = 0xffff;
	PR10 = 0xffff;
	PR01 = 0xffff;
	PR11 = 0xffff;
	PR02 = 0xffff;
	PR12 = 0xffff;
#if CHIP_RL78
	/* the RL78/G13's names: its UART receptions, the IICA and SAU2's reception (rl78.h) at
	   level 2 */
	SREPR01 = 0;
	SREPR11 = 1;
	IICAPR00 = 0;
	IICAPR10 = 1;
	SRPR02 = 0;
	SRPR12 = 1;
	SREPR02 = 0;
	SREPR12 = 1;
	SRPR04 = 0;
	SRPR14 = 1;
	SREPR04 = 0;
	SREPR14 = 1;
#else
	SRPR02 = 0;
	SRPR12 = 1;
	SREPR02 = 0;
	SREPR12 = 1;
	SRPR03 = 0;
	SRPR13 = 1;
	SREPR03 = 0;
	SREPR13 = 1;
	SRPR04 = 0;
	SRPR14 = 1;
	SREPR01 = 0;
	SREPR11 = 1;
#endif
	EGP0 = 0;
	EGP1 = 0;
	EGN0 = 0;
	EGN1 = 0;
}

void keepRamSetChecksum(void)
{
	unsigned int sum;
	unsigned char __far *p;
	unsigned char i;

	sum = 0;
	p = &keepRam.resetCause;
	for (i = 0; i < 0xfc; i++, p++)
		sum += *p;
	keepRam.checksum = 0xffff - sum;
	keepRam.magic = KEEP_RAM_MAGIC;
}

void setResetCause(unsigned char cause)
{
	keepRam.resetCause = cause;
}

unsigned char getResetCause(void)
{
	return keepRam.resetCause;
}

/* the kept RAM sealed, then an illegal instruction (0xff), which resets the chip; should
 * it not, the reset vector */
void resetSystem(void)
{
	DI();
	keepRamSetChecksum();
#asm
	DB	0FFH
	MOV	CS, #0
	MOV	ES, #0
	MOVW	AX, ES:!0
	BR	AX
#endasm
}

void resetChip(void)
{
#asm
	DB	0FFH
	MOV	CS, #0
	MOV	ES, #0
	MOVW	AX, ES:!0
	BR	AX
#endasm
}
#endif /* !ERNIE_UPDATER */
