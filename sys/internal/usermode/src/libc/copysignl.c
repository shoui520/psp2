/*
 * copysignl function: x with the sign of y in long double.
 */
#include <math.h>

#include "xmath.h"

long double (copysignl)(long double x, long double y)
{
	_Lval *px = (_Lval *)&x;

	px->_Sh[_L0] = (px->_Sh[_L0] & ~_LSIGN) | (((_Lval *)&y)->_Sh[_L0] & _LSIGN);
	return x;
}
