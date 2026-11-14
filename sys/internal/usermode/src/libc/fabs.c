/*
 * fabs function: the magnitude of x.
 */
#include <math.h>

#include "xmath.h"

double (fabs)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return _Inf._Double;
	case 0:
		return 0.0;
	default:
		return x < 0.0 ? -x : x;
	}
}
