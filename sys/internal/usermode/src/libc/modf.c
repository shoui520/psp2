/*
 * modf function: split x into its integer part, stored at pint, and its
 * fraction, returned with the sign of x.
 */
#include <math.h>

#include "xmath.h"

double (modf)(double x, double *pint)
{
	*pint = x;
	switch (_Dint(pint, 0)) {
	case _NANCODE:
		return x;
	case _INFCODE:
	case 0:
		return DSIGN(x) ? -_Zero : 0.0;
	default:	/* finite */
		return x - *pint;
	}
}
