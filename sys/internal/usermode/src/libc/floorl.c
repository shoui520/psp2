/*
 * floorl function: the greatest integer not above x in long double.
 */
#include <math.h>

#include "xmath.h"

long double (floorl)(long double x)
{
	long double y = x;

	return _LDint(&y, 0) < 0 && x < 0.0L ? y - 1.0L : y;
}
