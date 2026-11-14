/*
 * log2f function: the base-2 logarithm of x.
 */
#include <math.h>

#include "xmath.h"

float (log2f)(float x)
{
	return _FLog(x, -1);
}
