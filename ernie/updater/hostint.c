/*
 * The host's interrupt (INTP5, its frame start), as the updater has it.
 */
#pragma sfr

#include "ernie.h"
#include "host.h"

void hostIntDisable(void)
{
	PMK5 = 1;
}

void hostIntEnable(void)
{
	P5.7 = 0;
	PIF5 = 0;
	PMK5 = 0;
}
