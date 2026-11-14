/*
 * exp function: e to the x, through _Exp.
 */
#include <math.h>

#include "xmath.h"

double (exp)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return DSIGN(x) ? 0.0 : x;
	case 0:
		return 1.0;
	default:
		_Exp(&x, 1.0, 0);
		return x;
	}
}
