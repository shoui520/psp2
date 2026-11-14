/*
 * modfl function: split x into its integer part, stored at pint, and its
 * fraction, returned with the sign of x.
 */
#include <math.h>

#include "xmath.h"

long double (modfl)(long double x, long double *pint)
{
	*pint = x;
	switch (_LDint(pint, 0)) {
	case _NANCODE:
		return x;
	case _INFCODE:
	case 0:
		return LSIGN(x) ? -_LZero : 0.0L;
	default:	/* finite */
		return x - *pint;
	}
}
