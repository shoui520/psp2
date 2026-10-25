/*
 * The updater's power sequences: the application's power-on read of the devices and the
 * Elmo's mask steps (powerSeqOn, ...), with timer channel 3 waited for by polling, and the
 * host's lines and the keys' ports around them.
 */
#pragma sfr
#pragma NOP

#include "ernie.h"
#include "chip.h"
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

	result = level - trim;
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
}

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
	timerStart(0x18, 1);
}

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
	powerSeqOff();
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
	keyPortsDisable();
	powerWaitStop();
}

/* TM03 stopped */
void powerWaitStop(void)
{
	TT0L = 0x08;
	TMMK03 = 1;
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
