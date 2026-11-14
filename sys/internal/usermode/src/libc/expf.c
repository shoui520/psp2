/*
 * expf function: e to the x in float, through _FExp.
 */
#include <math.h>

#include "xmath.h"

float (expf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return FSIGN(x) ? 0.0F : x;
	case 0:
		return 1.0F;
	default:
		_FExp(&x, 1.0F, 0);
		return x;
	}
}
