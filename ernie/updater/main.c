/*
 * The updater's start: the clocks and interrupt priorities set before the C runtime starts
 * (hdwinit, which cstart calls), the SPI link to the host and the UARTs opened, and the
 * resets, which record their cause in the RAM kept over a reset as the application's do.
 */
#pragma interrupt INTRTC rtcInterrupt
#pragma sfr
#pragma DI
#pragma NOP

#include "chip.h"
#include "ernie.h"
#include "board.h"
#if SOURCE_0103
#include "clock.h"
#endif
#include "clockout.h"
#include "keepram.h"
#include "tau.h"
#include "timer.h"
#include "updater.h"
#if BOARD_USS1001 && SOURCE_0104
#include "battery.h"
#include "input.h"
#include "state.h"
#endif

/* UARTs' speeds: SPS's prescaler, SDR's divisor and the channels' SCR */
struct UartConfig {
	unsigned char	prescaler;
	unsigned char	divisor;
	unsigned int	scr;
};

void intInit(void);

/* at 0xfd900 (updater.lk), where the application keeps it too */
struct KeepRam keepRam;

#if !CHIP_RL78	/* the RL78's flash library keeps no buffer of the caller's */
/* the flash library's data buffer (FSL_Init's), after the library's own data (updater.lk) */
__near unsigned char fslDataBuffer[0x100];
#endif

/* the clocks: the X1 oscillator as the main clock, the internal oscillator stopped */
void hdwinit(void)
{
#if SOURCE_0103	/* the PS TV's: the application's (main.c's) */
	clockInit();
#else
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
#endif
	boardPortInit();	/* the slim's: app/uss1001.c's */
	intInit();
	tau0PowerOn();
	clockOutputStop();
	return;
}

/* called by cstart before main */
void startHook(void)
{
}

/* CSI00, the host's SPI link, the least significant bit first; kept when it is open */
int csiOpen(void)
{
	if (SOE0L.0)
		return 0;
	SAU0EN = 1;
	NOP();
	NOP();
	NOP();
	NOP();
	SPS0 &= 0xfff0;
	SMR00 = 0x0020;
	SCR00 = 0xf087;
	SDR00 = 0x0600;
	SO0 = 0x0e0e;
	SOE0L.0 = 1;
	P1.2 = 1;
	PM1.2 = 0;
	PM1.1 = 1;
	P1.0 = 1;
	PM1.0 = 0;
	P90 = 1;
	PM90 = 0;
	SS0L.0 = 1;
	CSIIF00 = 0;
	return 0;
}

void csiClose(void)
{
	ST0L.0 = 1;
	CSIMK00 = 1;
	CSIIF00 = 0;
	SOE0L.0 = 0;
	P1.0 = 0;
	P1.1 = 0;
	P1.2 = 0;
	P90 = 0;
}

#if !SOURCE_0103	/* the PS TV's has none */
/* UART2 (channels 0 and 1 of SAU1) and UART3 (channels 2 and 3), their interrupts masked */
int uartsOpen(void)
{
	struct UartConfig config[2] = { { 4, 0x19, 0x0097 }, { 1, 0x22, 0x0097 } };

	SAU1EN = 1;
	NOP();
	NOP();
	NOP();
	NOP();
	SPS1 = (unsigned char)(config[1].prescaler << 4 | config[0].prescaler);
	SMR10 = 0x0023;
	SMR11 = 0x0122;
	SCR10 = config[0].scr | 0x8000;
	SCR11 = config[0].scr | 0x4400;
	SDR10 = (unsigned int)config[0].divisor << 8;
	SDR11 = (unsigned int)config[0].divisor << 8;
	SMR12 = 0x8023;
	SMR13 = 0x8122;
	SCR12 = config[1].scr | 0x8000;
	SCR13 = config[1].scr | 0x4400;
	SDR12 = (unsigned int)config[1].divisor << 8;
	SDR13 = (unsigned int)config[1].divisor << 8;
	SOL1 = 0x0000;
	SO1 = 0x0f0f;
	SOE1L.2 = 1;
	SIR11L = 0x07;
	SIR13L = 0x07;
	PU1.4 = 1;
	PM1.4 = 1;
	PM1.3 = 0;
	P1.3 = 1;
	NFEN0.6 = 1;
	SS1L.2 = 1;
	SS1L.3 = 1;
	return 0;
}
#endif

#if !FW_SINCE(0x0100060D)	/* 0x0100060D carries none */
/* the updater's banner, printed nowhere */
const char __far updaterBanner[] = "\r\n\n\nWelcome to Ernie Downloader. (0x01.0x00.0x33.0x00)\r\n";
#endif

/* every interrupt masked and at the lowest priority */
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
#if BOARD_USS1001 && SOURCE_0104	/* the application's: the UART receptions, the IICA and
					   SAU2's reception at level 2 */
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

/* every 0.5 s: the kept clock counts, and timers 0x15-0x1d whose time came run */
#if FW_SINCE(0x0100060D)
/* since 0x0100060D the RTC's timers count timerRtcTicks, as the application's do */
void rtcInterrupt(void)
{
	unsigned char i;

	keepRam.clock++;
	timerRtcTicks++;
	for (i = TIMER_RTC_FIRST; i < TIMER_COUNT; i++) {
		if (timers[i].state == 1 && timers[i].time == timerRtcTicks) {
			timers[i].state = 0;
#if SOURCE_0103	/* the PS TV's: a timer without a handler runs out silently */
			if (timers[i].handler != 0)
#endif
				timers[i].handler();
		}
	}
#if BOARD_USS1001 && SOURCE_0104
	/* once a second the battery's measurement runs (ask5): in state 0x0004 while not
	   charging only every tenth second, otherwise each */
	if (keepRam.clock & 1) {
		switch (stateGet()) {
		case 0x0004:
			if (batteryIsCharging()) {
				inputMeasure.ask5 = 1;
				inputMeasure.holdOff = 0;
			} else {
				if (inputMeasure.holdOff != 0)
					inputMeasure.holdOff--;
				if (inputMeasure.holdOff == 0) {
					inputMeasure.ask5 = 1;
					inputMeasure.holdOff = 10;
				}
			}
			break;
		default:
			inputMeasure.ask5 = 1;
			inputMeasure.holdOff = 0;
			break;
		}
	}
#endif
}
#else
void rtcInterrupt(void)
{
	unsigned int clock;
	unsigned char i;

	keepRam.clock++;
	clock = (unsigned int)keepRam.clock;
	for (i = TIMER_RTC_FIRST; i < TIMER_COUNT; i++) {
		if (timers[i].state == 1 && timers[i].time == clock) {
			timers[i].state = 0;
			timers[i].handler();
		}
	}
}
#endif
