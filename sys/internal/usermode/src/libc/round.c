/*
 * round function: x rounded to the nearest integer, halfway cases away
 * from zero.
 */
#include <math.h>

#include "xmath.h"

double (round)(double x)
{
	switch (_Dint(&x, 1)) {	/* keep the halves */
	case _NANCODE:
	case _INFCODE:
		return x;
	default:
		if (_Dint(&x, 0) != 0)	/* a half: away from zero */
			x = DSIGN(x) ? x - 1.0 : x + 1.0;
		return x;
	}
}
