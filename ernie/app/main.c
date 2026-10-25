/*
 * The application's start: the clocks and interrupt priorities set before the C runtime
 * starts (hdwinit, which cstart calls), main, and the resets, which record their cause in
 * the RAM kept over a reset.
 */
#pragma sfr
#pragma DI

#include "ernie.h"
#include "keepram.h"

extern void sub_27651(void);
extern void sub_10850(void);
extern void sub_10FC9(void);
extern void sub_16E7D(void);
extern void sub_1488D(void);

void intInit(void);

/* called by the kernel once it has made its objects, before the first task runs */
void kernelInitHook(void)
{
}

/* the clocks: the X1 oscillator as the main clock, the internal oscillator stopped */
void hdwinit(void)
{
	CMC = 0x53;
	CSC = 0xc0;
	CKC = 0x09;
	OSMC = 0x01;
	OSTS = 0x01;
	MSTOP = 0;
	do {
	} while (OSTC != 0xc0);
	CKC = 0x18;
	while (!MCS)
		;
	HIOSTOP = 1;
	XTSTOP = 0;
	sub_27651();
	intInit();
	sub_10850();
	sub_10FC9();
	sub_16E7D();
}

void main(void)
{
	sub_1488D();
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
