/*
 * USS-1001's serial lines on the multi-connector. serialLineSet connects or disconnects a set of
 * them (SERIAL_LINES_HOST or SERIAL_LINES_ALL): a line's output pin is driven high and made an
 * output, its input pin an input; disconnected, the pins are inputs. What is connected is kept
 * in serialLines and in SERIALSEL.
 */
#pragma sfr

#include "ernie.h"
#include "serialline.h"

unsigned char serialLines;

unsigned char serialLineInit(void)
{
	serialLines = 0;
	SERIALSEL = 0;
	return 0;
}

unsigned char serialLineGet(void)
{
	return SERIALSEL;
}

/* 1 for any other set */
unsigned char serialLineSet(unsigned char lines, unsigned char on)
{
	if (on == 0) {
		if (lines == SERIAL_LINES_HOST) {
			PM11.2 = 1;
			PM11.3 = 1;
			serialLines &= ~SERIAL_LINES_HOST;
		} else if (lines == SERIAL_LINES_ALL) {
			PM11.2 = 1;
			PM11.3 = 1;
			PM3.6 = 1;
			PM3.7 = 1;
			serialLines &= ~SERIAL_LINES_ALL;
		} else
			return 1;
	} else {
		if (lines == SERIAL_LINES_HOST) {
			P11.3 = 1;
			PM11.3 = 0;
			PM11.2 = 1;
			P9.2 = 1;
			PM9.3 = 1;
			PM9.2 = 0;
			serialLines |= SERIAL_LINES_HOST;
		} else if (lines == SERIAL_LINES_ALL) {
			PM3.6 = 1;
			P11.3 = 1;
			PM3.6 = 0;
			PM11.3 = 0;
			PM3.7 = 1;
			PM11.2 = 1;
			P3.5 = 1;
			P9.2 = 1;
			PM3.4 = 1;
			PM9.3 = 1;
			PM3.5 = 0;
			PM9.2 = 0;
			serialLines |= SERIAL_LINES_ALL;
		} else
			return 1;
	}
	SERIALSEL = serialLines;
	return 0;
}
