/*
 * log10f function: the base-10 logarithm of x.
 */
#include <math.h>

#include "xmath.h"

float (log10f)(float x)
{
	return _FLog(x, 1);
}
