/*
 * atan function: the arctangent, through _Atan of |x| or its reciprocal.
 */
#include <math.h>

#include "xmath.h"

static const double piby2 = 1.57079632679489661923;	/* pi / 2 */

double (atan)(double x)
{
	int idx;

	switch (_Dtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		return DSIGN(x) ? -piby2 : piby2;
	default:
		idx = 0;
		if (x < 0.0) {
			x = -x;
			idx = 8;
		}
		if (1.0 < x) {
			x = 1.0 / x;
			idx ^= 2;
		}
		return _Atan(x, idx);
	}
}
