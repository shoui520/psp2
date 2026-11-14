/*
 * modff function: split x into its integer part, stored at pint, and its
 * fraction, returned with the sign of x.
 */
#include <math.h>

#include "xmath.h"

float (modff)(float x, float *pint)
{
	*pint = x;
	switch (_FDint(pint, 0)) {
	case _NANCODE:
		return x;
	case _INFCODE:
	case 0:
		return FSIGN(x) ? -_FZero : 0.0F;
	default:	/* finite */
		return x - *pint;
	}
}
