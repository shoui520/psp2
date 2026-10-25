/*
 * The clock output: PCLBUZ0 on P14.0, at CKS0's selection 8 (the subsystem clock, fSUB).
 */
#pragma sfr

#include "clockout.h"

/* starts the clock on P14.0 */
void clockOutputStart(void)
{
	PCLOE0 = 0;
	CKS0 = 0x08;
	PM14.0 = 0;
	PCLOE0 = 1;
}

/* stops the clock; P14.0 becomes an input */
void clockOutputStop(void)
{
	PCLOE0 = 0;
	PM14.0 = 1;
}
