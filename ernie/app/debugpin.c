/*
 * P4.0 (TOOL0), driven for a probe to watch: low from systemInit, then task 3's scans toggle it.
 */
#pragma sfr

#include "ernie.h"
#include "board.h"
#include "debugpin.h"

void debugPinInit(void)
{
	P4.0 = 0;
#if BOARD_IRS1001
	/* IRS-1001's other probe pins */
	P12 &= ~0x80;
	P4.2 = 0;
	P4.3 = 0;
	P4.4 = 0;
	P4.5 = 0;
	P5.4 = 0;
	P5.5 = 0;
#endif
}

/* never called; on IRS-1001 bits 1 and 0 of value to P12.7 and P4.2 */
void debugPinReserved(unsigned int value)
{
#if BOARD_IRS1001
	if ((unsigned char)value & 2)
		P12 |= 0x80;
	else
		P12 &= ~0x80;
	P4.2 = ((unsigned char)value & 1) ? 1 : 0;
#endif
}

/* bit 0 of level */
void debugPinSet(unsigned int level)
{
#if FW_SINCE(0x0100060B)
	P4.0 = ((unsigned char)level & 1) ? 1 : 0;
#else
	P4.0 = level & 1;
#endif
}
