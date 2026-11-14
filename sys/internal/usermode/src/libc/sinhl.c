/*
 * sinhl function: the hyperbolic sine of x.
 */
#include <math.h>

#include "xmath.h"

long double (sinhl)(long double x)
{
	return _LSinh(x, 1.0L);
}
