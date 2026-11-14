/*
 * _Fpcomp function: compare x and y for the comparison macros: _FP_LT, _FP_EQ
 * or _FP_GT, or 0 when they are unordered.
 */
#include <math.h>

#include "xmath.h"

int _Fpcomp(double x, double y)
{
	if (_Dtest(&x) == _NANCODE || _Dtest(&y) == _NANCODE)
		return 0;
	else if (x < y)
		return _FP_LT;
	else if (x > y)
		return _FP_GT;
	else
		return _FP_EQ;
}
