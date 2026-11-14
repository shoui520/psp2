/*
 * copysign function: x with the sign of y.
 */
#include <math.h>

#include "xmath.h"

double (copysign)(double x, double y)
{
	_Dval *px = (_Dval *)&x;

	px->_Sh[_D0] = (px->_Sh[_D0] & ~_DSIGN) | (((_Dval *)&y)->_Sh[_D0] & _DSIGN);
	return x;
}
