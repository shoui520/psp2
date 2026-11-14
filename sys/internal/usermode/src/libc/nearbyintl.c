/*
 * nearbyintl function: x rounded to an integer in the current rounding
 * mode, without raising inexact.
 */
#include <math.h>

#include "xmath.h"

long double (nearbyintl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
	case _INFCODE:
	case 0:
		return x;
	default:	/* finite */
		return _LRint(x);
	}
}
