/*
 * roundl function: x rounded to the nearest integer, halfway cases away
 * from zero.
 */
#include <math.h>

#include "xmath.h"

long double (roundl)(long double x)
{
	switch (_LDint(&x, 1)) {	/* keep the halves */
	case _NANCODE:
	case _INFCODE:
		return x;
	default:
		if (_LDint(&x, 0) != 0)	/* a half: away from zero */
			x = LSIGN(x) ? x - 1.0L : x + 1.0L;
		return x;
	}
}
