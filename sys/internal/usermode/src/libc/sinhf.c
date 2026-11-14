/*
 * sinhf function: the hyperbolic sine of x.
 */
#include <math.h>

#include "xmath.h"

float (sinhf)(float x)
{
	return _FSinh(x, 1.0F);
}
