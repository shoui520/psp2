/*
 * logf function: the natural logarithm of x.
 */
#include <math.h>

#include "xmath.h"

float (logf)(float x)
{
	return _FLog(x, 0);
}
