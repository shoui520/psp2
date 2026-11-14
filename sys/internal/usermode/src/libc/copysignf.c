/*
 * copysignf function: x with the sign of y in float.
 */
#include <math.h>

#include "xmath.h"

float (copysignf)(float x, float y)
{
	_Fval *px = (_Fval *)&x;

	px->_Sh[_F0] = (px->_Sh[_F0] & ~_FSIGN) | (((_Fval *)&y)->_Sh[_F0] & _FSIGN);
	return x;
}
