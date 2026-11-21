/*
 * The updater's power sequences: the application's power-on read of the devices and the
 * Elmo's mask steps (powerSeqOn, ...), with timer channel 3 waited for by polling, and the
 * host's lines and the keys' ports around them.
 */
#pragma sfr
#pragma NOP
#if ERNIE_FW >= 0x01030000
#pragma interrupt INTP8 intp8Interrupt
#endif

#include "ernie.h"
#include "chip.h"
#include "charge.h"
#include "elmo.h"
#include "powerseq.h"
#include "timer.h"
#include "updater.h"

/* an Elmo register read back at the power on, and where it goes */
struct ElmoRegRead {
	unsigned char	reg;
	unsigned int	*value;
};

unsigned char powerCleared[2];		/* cleared at the init and the resume, read nowhere */
struct DeviceVersions deviceVersions;
unsigned char elmoCommand[12];

static const struct ElmoRegRead __far elmoRegRead[] = {
	{ 0xa0, &elmoStatusMask.reg[0] },
	{ 0xa1, &elmoStatusMask.reg[1] },
	{ 0xa2, &elmoStatusMask.reg[2] },
	{ 0xa3, &elmoStatusMask.reg[3] },
	{ 0xa4, &elmoStatusMask.reg[4] },
	{ 0xa5, &elmoLed },
	{ 0xa6, &elmoRegA6 },
	{ 0xa7, &elmoRegA7 },
	{ 0xaa, &elmoRegAA },
	{ 0xab, &elmoRegAB },
};

/* level less trim, or 0 when that is 0x40 or more */
unsigned char levelTrim(unsigned char level, unsigned char trim)
{
	unsigned char result;

#if SOURCE_0103
	/* the PS TV's: the application's (serial2.c's), the negated trim added */
	result = -trim + level;
	if (result < 0)
		result = 0;
#else
	result = level - trim;
#endif
	if (result >= 0x40)
		result = 0;
	return result;
}

/* TM03 counting at 31.25 kHz, its interrupt masked */
void powerInit(void)
{
	powerCleared[0] = 0;
	powerCleared[1] = 0;
	TMR03 = 0x8008;
	TDR03 = 0;
	TMMK03 = 1;
#if SOURCE_0103
	/* the PS TV's: INTP8 on both edges, then event 0x8c */
	EGP1.0 = 1;
	EGN1.0 = 1;
	IF2L.5 = 0;
	MK2L.5 = 0;
	updEventSet(0x8c);
	return;
#endif
}

#if SOURCE_0103
/* INTP8 (the PS TV's): event 0x8c again */
void intp8Interrupt(void)
{
	updEventSet(0x8c);
}
#endif

/* the devices read, and the Elmo's registers read back */
void powerOn(void)
{
	unsigned char result;
	unsigned int i;

	powerWaitInit();
	powerSeqOn();
	powerWaitStop();
	powerSeqInit();
	for (i = 0; i < 10; i++)
		result = elmoRead(&elmoRegRead[i].reg, (unsigned char *)elmoRegRead[i].value);
#if SOURCE_0103
	timerStart(TIMER_RTC(0x17), 10);
#else
	timerStart(TIMER(0x18), 1);
#endif
}

#if SOURCE_0103
/* the PS TV's: CSI00 opened for the host, the most significant bit first (the application's
   csiOpenMsb without its state), P5.7 low and P1.6 an input */
void powerCsiOpen(void)
{
	SAU0EN = 1;
	NOP();
	NOP();
	NOP();
	NOP();
	SPS0L = 0xf0 & SPS0L;
	SMR00 = 0x0020;
	SCR00 = 0xf007;
	SDR00 = 0x0600;
	SO0 = 0x0e0e;
	SOE0L.0 = 1;
	P1.0 = 1;
	PM1.0 = 0;
	P1.1 = 1;
	PM1.1 = 1;
	P1.2 = 1;
	PM1.2 = 0;
	P5.7 = 0;
	PM5.7 = 0;
	P1.6 = 0;
	PM1.6 = 1;
	P90 = 1;
	PM90 = 0;
	SS0L.0 = 1;
	CSIIF00 = 0;
}

/* the resume's steps; in modes 1 and 2 the Elmo's bit, the host's CSI and the device's power
   (P5.6 off, P5.7 on) too; then the host powered (P1.7) */
void powerResume(unsigned char mode)
{
	powerSeqResume();
	if (mode == 1 || mode == 2) {
		powerSdOn();
		powerCsiOpen();
		P5.6 = 0;
		P5.7 = 1;
	}
	powerWaitInit();
	P91 = 0;
	powerDelayMs(1);
	P1.7 = 1;
	powerDelayMs(1);
	powerDelayMs(100);
	powerWaitStop();
	powerCleared[0] = 0;
	powerCleared[1] = 0;
	keyPortsEnable();
}

void powerOff(void)
{
	P1.7 = 0;
	powerSeqOff();
	return;
}
#else

/* the resume's steps, then the host powered (P1.7) and the keys' ports inputs */
void powerResume(void)
{
	powerSeqResume();
	powerWaitInit();
	P91 = 0;
	P92 = 0;
	P1.5 = 0;
	powerDelayMs(1);
	P1.7 = 1;
	powerDelayMs(1);
	powerDelayMs(100);
	powerWaitStop();
	powerCleared[0] = 0;
	powerCleared[1] = 0;
	keyPortsEnable();
}

void powerOff(void)
{
	P1.7 = 0;
#if FW_SINCE(0x0100060D)
	powerBbmcOff();
	powerSeqOff();
	powerBbmcOn();
	if (chargeUsbMode == 3) {	/* the USB charging's mode 3 back to 1 */
		chargeUsbMode = 1;
		chargeSetCurrent();
	}
#else
	powerSeqOff();
#endif
	keyPortsDisable();
}

void powerSuspend(void)
{
	powerWaitInit();
	P91 = 1;
	P92 = 1;
	P1.5 = 1;
	powerDelayMs(1);
	P1.7 = 0;
	powerSeqSuspend();
#if FW_SINCE(0x0100060D)
	if (chargeUsbMode == 3) {	/* the USB charging's mode 3 back to 1 */
		chargeUsbMode = 1;
		chargeSetCurrent();
	}
#endif
	keyPortsDisable();
	powerWaitStop();
}
#endif /* SOURCE_0103 */

/* TM03 stopped */
void powerWaitStop(void)
{
	TT0L = 0x08;
#if !SOURCE_0103	/* the PS TV's leaves TM03's interrupt as it is */
	TMMK03 = 1;
#endif
}

/* ms on TM03, which runs */
void powerDelayMs(unsigned int ms)
{
	TMIF03 = 0;
	TDR03 = (unsigned long)ms * 1000 / 32 + (ms * 1000 % 32 ? 1 : 0);
	TS0L = 0x08;
	while (!TMIF03)
		NOP();
}

#if !SOURCE_0103	/* the PS TV's: board.c's, empty */
/* P5.0-3, P8 and P11.2-3 inputs with their pull-ups (the keys) */
void keyPortsEnable(void)
{
	PU5 = 0x0f | PU5;
	PM5 = 0x0f | PM5;
	PU8 = 0xff;
	PM8 = 0xff;
	PU11 = 0x0c | PU11;
	PM11 = 0x0c | PM11;
}

/* and back to outputs at 0 */
void keyPortsDisable(void)
{
	P5 = 0xf0 & P5;
	PM5 = 0xf0 & PM5;
	P8 = 0;
	PM8 = 0;
	P11 = 0xf3 & P11;
	PM11 = 0xf3 & PM11;
}
#endif
