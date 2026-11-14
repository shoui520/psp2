/*
 * nextafter function: the next representable value after x in the
 * direction of y.
 */
#include <math.h>

#include "xmath.h"

double (nextafter)(double x, double y)
{
	return nexttoward(x, y);
}
