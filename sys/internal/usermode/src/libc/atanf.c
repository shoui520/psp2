/*
 * atanf function: the arctangent in float, through _FAtan of |x| or its reciprocal.
 */
#include <math.h>

#include "xmath.h"

static const float piby2 = 1.57079632679489661923F;	/* pi / 2 */

float (atanf)(float x)
{
	int idx;

	switch (_FDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		return FSIGN(x) ? -piby2 : piby2;
	default:
		idx = 0;
		if (x < 0.0F) {
			x = -x;
			idx = 8;
		}
		if (1.0F < x) {
			x = 1.0F / x;
			idx ^= 2;
		}
		return _FAtan(x, idx);
	}
}
