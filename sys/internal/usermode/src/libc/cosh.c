/*
 * cosh function: the hyperbolic cosine.
 */
#include <math.h>

#include "xmath.h"

double (cosh)(double x)
{
	return _Cosh(x, 1.0);
}
