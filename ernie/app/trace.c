/*
 * The debug trace on UART3: text and bytes written while the UART3 link is in its debug mode,
 * each under a mask of categories that traceDisable turns off.
 */
#include "ernie.h"
#include "kernel.h"
#include "serial.h"
#include "trace.h"
#include "uart.h"

unsigned int traceMask;			/* the categories not written */

/* every category on, and UART3's timeout posting event 0x1000 of flag 6 */
int traceInit(void)
{
	struct UartTimeout timeout;

	traceMask = 0;
	timeout.timer = 1;
	timeout.timeout = 1;
	timeout.flgid = 6;
	timeout.pattern = 0x1000;
	if (uartSetTimeout(TRACE_PORT, &timeout))
		;
	return 0;
}

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
