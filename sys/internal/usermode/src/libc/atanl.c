/*
 * atanl function: the arctangent in long double, through _LAtan of |x| or its reciprocal.
 */
#include <math.h>

#include "xmath.h"

static const long double piby2 = 1.57079632679489661923L;	/* pi / 2 */

long double (atanl)(long double x)
{
	int idx;

	switch (_LDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		return LSIGN(x) ? -piby2 : piby2;
	default:
		idx = 0;
		if (x < 0.0L) {
			x = -x;
			idx = 8;
		}
		if (1.0L < x) {
			x = 1.0L / x;
			idx ^= 2;
		}
		return _LAtan(x, idx);
	}
}
