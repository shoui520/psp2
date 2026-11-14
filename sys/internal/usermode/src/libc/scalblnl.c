/*
 * scalblnl function: x times 2 to the xexp.
 */
#include <math.h>

#include "xmath.h"

long double (scalblnl)(long double x, long xexp)
{
	if (xexp != 0 && _LDtest(&x) < 0) {	/* finite and nonzero */
		switch (_LDscale(&x, xexp)) {
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
