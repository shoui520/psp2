/*
 * expl function: e to the x in long double, through _LExp.
 */
#include <math.h>

#include "xmath.h"

long double (expl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return LSIGN(x) ? 0.0L : x;
	case 0:
		return 1.0L;
	default:
		_LExp(&x, 1.0L, 0);
		return x;
	}
}
