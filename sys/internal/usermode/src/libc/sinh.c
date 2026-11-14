/*
 * sinh function: the hyperbolic sine of x.
 */
#include <math.h>

#include "xmath.h"

double (sinh)(double x)
{
	return _Sinh(x, 1.0);
}
