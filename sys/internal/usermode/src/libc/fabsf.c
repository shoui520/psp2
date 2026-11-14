/*
 * fabsf function: the magnitude of x in float.
 */
#include <math.h>

#include "xmath.h"

float (fabsf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return _FInf._Float;
	case 0:
		return 0.0F;
	default:
		return x < 0.0F ? -x : x;
	}
}
