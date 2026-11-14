/*
 * _LFpcomp function: compare x and y for the comparison macros: _FP_LT, _FP_EQ
 * or _FP_GT, or 0 when they are unordered.
 */
#include <math.h>

#include "xmath.h"

int _LFpcomp(long double x, long double y)
{
	if (_LDtest(&x) == _NANCODE || _LDtest(&y) == _NANCODE)
		return 0;
	else if (x < y)
		return _FP_LT;
	else if (x > y)
		return _FP_GT;
	else
		return _FP_EQ;
}
