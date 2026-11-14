/*
 * coshf function: the hyperbolic cosine in float.
 */
#include <math.h>

#include "xmath.h"

float (coshf)(float x)
{
	return _FCosh(x, 1.0F);
}
