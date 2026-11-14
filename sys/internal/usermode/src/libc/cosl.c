/*
 * cosl function: the cosine in long double, as the sine a quadrant on.
 */
#include <math.h>

#include "xmath.h"

long double (cosl)(long double x)
{
	return _LSin(x, 1);
}
