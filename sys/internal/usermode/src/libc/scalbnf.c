/*
 * scalbnf function: x times 2 to the xexp.
 */
#include <math.h>

#include "xmath.h"

float (scalbnf)(float x, int xexp)
{
	if (xexp != 0 && _FDtest(&x) < 0) {	/* finite and nonzero */
		switch (_FDscale(&x, xexp)) {
		case 0:
			_Feraise(_FE_UNDERFLOW);
			break;
		case _INFCODE:
			_Feraise(_FE_OVERFLOW);
			break;
		}
	}
	return x;
}
