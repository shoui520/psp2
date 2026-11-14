/*
 * cos function: the cosine, as the sine a quadrant on.
 */
#include <math.h>

#include "xmath.h"

double (cos)(double x)
{
	return _Sin(x, 1);
}
