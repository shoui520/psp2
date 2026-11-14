/*
 * The debug trace on UART3: text and bytes written while the UART3 link is in its debug mode,
 * each under a mask of categories that traceDisable turns off; before 0x01000104 also a
 * debug console nothing calls.
 */
#pragma sfr

#include "ernie.h"
#include "kernel.h"
#include "serial.h"
#include "trace.h"
#include "uart.h"
#if !FW_SINCE(0x01000104)
#include <string.h>

#include "bbmc.h"
#include "keepram.h"
#include "rtc.h"
#include "state.h"
#include "touch.h"
#include "util.h"
#include "version.h"
#endif

#if !FW_SINCE(0x01000104)
/* before 0x01000104 the debug console: a line of two-letter commands read from UART3.
   Nothing calls it; its line was left one byte long, so the command's second letter is the
   length's byte and the arguments are read from the bytes after it (traceMask's). */

/* a console command's name and how many of its bytes are compared */
struct TraceCommand {
	const char	*name;
	unsigned char	length;
};

static unsigned char traceLine[1];
static unsigned char traceLineLength;
#endif

/* 0x00090903's banner is left unused; 0x00090804's traceInit prints it */
#if FW_SINCE(0x01000104)
#elif FW_SINCE(0x00090903)
const char __far traceBanner[] = "\r\nWelcome to Ernie dbgcon";
const char __far traceNewline[] = "\r\n";
#else
const char __far traceBanner[] = "\r\n\n\nWelcome to Ernie. ";
const char __far traceNewline[] = "\r\n";
#endif

unsigned int traceMask;			/* the categories not written */

#if !FW_SINCE(0x01000104)
static void traceConsoleTouch(unsigned char device, unsigned char state);
static void traceConsoleDb(unsigned char *line, unsigned char length);
static void traceConsoleBb(unsigned char *line, unsigned char length);
#endif

/* every category on, and UART3's timeout posting event 0x1000 of flag 6 */
int traceInit(void)
{
	struct UartTimeout timeout;

	traceMask = 0;
#if !FW_SINCE(0x00090903)
	dly_tsk(0xfa);
	tracePrintf(traceBanner);
	tracePrintf(traceNewline);
#endif
	timeout.timer = 1;
	timeout.timeout = 1;
	timeout.flgid = 6;
	timeout.pattern = 0x1000;
	if (uartSetTimeout(TRACE_PORT, &timeout))
		;
	return 0;
}

#if !FW_SINCE(0x01000104)
/* the console's prompt */
void traceConsolePrompt(void)
{
	tracePrintf("\nDBG>");
}

/* the console's line emptied */
void traceConsoleInit(void)
{
	traceLineLength = 0;
	memset(traceLine, 0, sizeof(traceLine));
}

/* runs the console's line: a command of two letters and its arguments */
void traceConsoleCommand(void)
{
	unsigned int command;
	unsigned char version[4];

	if (traceLineLength < 2)
		return;
	tracePrintf("\n");
	command = (unsigned int)traceLine[0] << 8 | traceLine[1];
	switch (command) {
	case 0x7672:	/* "vr" */
		getErnieVersion(version);
		tracePrintf("version is \r\n");
		tracePrintf(" Ernie : %02X.%02X.%02X.%02X\r\n", version[0], version[1], version[2],
			    version[3]);
		getHardwareInfo(version);
		tracePrintf(" HWinfo: %02X.%02X.%02X.%02X \r\n", version[0], version[1],
			    version[2], version[3]);
		break;
	case 0x6d64:	/* "md" */
		tracePrintf("ErnieMode is 0x%04X\r", stateGet());
		break;
	case 0x7374:	/* "st" */
		tracePrintf("status is ----\r");
		break;
	case 0x5450:	/* "TP": a touch panel on, off or reset */
		traceConsoleTouch(traceLine[3], traceLine[4]);
		break;
	case 0x6462:	/* "db" */
		traceConsoleDb(&traceLine[2], traceLineLength - 2);
		break;
	case 0x6262:	/* "bb" */
		traceConsoleBb(&traceLine[2], traceLineLength - 2);
		break;
	case 0x5242:	/* "RB" */
		tracePrintf("rebooting...\r\n");
		stateSerialLock();
		dly_tsk(0xfa);
		tracePrintf("2...\r\n");
		setResetCause(0xf1);
		resetSystem();
		break;
	case 0x636c:	/* "cl" */
		tracePrintf("ErnieClockTick is %08lXh\r\n", getClock().value);
		break;
	default:
		tracePrintf("Error:unknown Command\r");
		break;
	}
}
#endif

void traceDisable(unsigned int mask)
{
	traceMask |= mask;
}

void traceEnable(unsigned int mask)
{
	traceMask &= ~mask;
}

char traceIsEnabled(unsigned int mask)
{
	return (traceMask & mask) ? 0 : 1;
}

unsigned int traceGetMask(void)
{
	return traceMask;
}

unsigned char traceLength(const char *text)
{
	unsigned char length;

	length = 0;
	while (*text) {
		text++;
		length++;
	}
	return length;
}

void tracePuts(unsigned int mask, const char *text)
{
	unsigned char length;

	if (traceMask & mask)
		return;
	length = traceLength(text);
	traceUartWrite((unsigned char *)text, length);
}

void traceWrite(unsigned int mask, unsigned char *data, unsigned char size)
{
	if (traceMask & mask)
		return;
	traceUartWrite(data, size);
}

void tracePrintf(const char *format, ...)
{
}

unsigned char traceUartWrite(unsigned char *data, unsigned char size)
{
	if (serial.mode == 3)
		return uartWrite(TRACE_PORT, data, size);
	return 0;
}

unsigned char traceUartRead(unsigned char *data, unsigned char size)
{
	if (serial.mode == 3)
		return uartRead(TRACE_PORT, data, size);
	return 0;
}

#if !FW_SINCE(0x01000104)
/* "TP": touch panel device ('0' or '1') off ('0'), out of reset ('1') or on ('2') */
static void traceConsoleTouch(unsigned char device, unsigned char state)
{
	if (device == '0') {
		switch (state) {
		case '0':
			touchDisable0();
			P0.6 = 0;
			break;
		case '1':
			P0.6 = 1;
			break;
		case '2':
			touchEnable0();
			break;
		}
	} else if (device == '1') {
		switch (state) {
		case '0':
			touchDisable1();
			P0.5 = 0;
			break;
		case '1':
			P0.5 = 1;
			break;
		case '2':
			touchEnable1();
			break;
		}
	}
}

/* "db": the trace's mask set ("dbmask s"), cleared ("dbmask c") or shown ("dbmask") */
static void traceConsoleDb(unsigned char *line, unsigned char length)
{
	struct TraceCommand commands[3] = {
		{ "mask s ", 7 },
		{ "mask c ", 7 },
		{ "mask", 4 },
	};
	unsigned char i;
	unsigned int mask;
	unsigned char digits;

	mask = 0;
	for (i = 0; i < 3; i++)
		if (memcmp(line, commands[i].name, commands[i].length) == 0)
			break;
	digits = (unsigned char)(length - 7) >> 1;
	switch (i) {
	case 0:
		if (hexDecode((unsigned char *)&mask, (const char *)line + 7, digits) == 0) {
			if (digits == 2)
				mask = mask >> 8 | mask << 8;
			traceDisable(mask);
			tracePrintf(" mask=0x%04X\r\n", traceGetMask());
		}
		break;
	case 1:
		if (hexDecode((unsigned char *)&mask, (const char *)line + 7, digits) == 0) {
			if (digits == 2)
				mask = mask >> 8 | mask << 8;
			traceEnable(mask);
			tracePrintf(" mask=0x%04X\r\n", traceGetMask());
		}
		break;
	case 2:
		tracePrintf("=0x%04X\r\nusage\n\r >dbmask s 0001 (mask set 0x0001)\r\n"
			    " >dbmask c 00ff (mask clr 0x00ff)\r\n", traceGetMask());
		break;
	default:
		tracePrintf("db commands\r\ndbmask\r\n");
		break;
	}
}

/* "bb": "bbmem" runs bbmcDelay */
static void traceConsoleBb(unsigned char *line, unsigned char length)
{
	struct TraceCommand commands[1] = {
		{ "mem", 3 },
	};
	unsigned char i;

	for (i = 0; i < 1; i++)
		if (memcmp(line, commands[i].name, commands[i].length) == 0)
			break;
	if (i == 0)
		bbmcDelay();
	else
		tracePrintf("bb commands\r\n*bbmem\r\n");
}
#endif
