/*
 * P4.0 (TOOL0), driven for a probe to watch: low from systemInit, then task 3's scans toggle it.
 */
#pragma sfr

#include "ernie.h"
#include "debugpin.h"

void debugPinInit(void)
{
	P4.0 = 0;
}

/* never called */
void debugPinReserved(unsigned int value)
{
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
