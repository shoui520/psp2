/*
 * coshl function: the hyperbolic cosine in long double.
 */
#include <math.h>

#include "xmath.h"

long double (coshl)(long double x)
{
	return _LCosh(x, 1.0L);
}
