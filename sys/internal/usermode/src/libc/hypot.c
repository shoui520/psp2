/*
 * hypot function: sqrt(x^2 + y^2) without undue overflow or underflow,
 * through _Hypot, which returns the root scaled by 2^-xexp.
 */
#include <math.h>

#include "xmath.h"

double (hypot)(double x, double y)
{
	int xexp;

	x = _Hypot(x, y, &xexp);
	if (xexp != 0) {
		switch (_Dscale(&x, xexp)) {
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
