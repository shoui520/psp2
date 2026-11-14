/*
 * nextafterf function: the next representable value after x in the
 * direction of y.
 */
#include <math.h>

#include "xmath.h"

float (nextafterf)(float x, float y)
{
	return nexttowardf(x, (long double)y);
}
