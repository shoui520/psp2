/*
 * hypotl function: sqrt(x^2 + y^2) without undue overflow or underflow in long double,
 * through _LHypot, which returns the root scaled by 2^-xexp.
 */
#include <math.h>

#include "xmath.h"

long double (hypotl)(long double x, long double y)
{
	int xexp;

	x = _LHypot(x, y, &xexp);
	if (xexp != 0) {
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
