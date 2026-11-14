/*
 * ceil function: the least integer not below x.
 */
#include <math.h>

#include "xmath.h"

double (ceil)(double x)
{
	double y = x;

	return _Dint(&y, 0) < 0 && 0.0 < x ? y + 1.0 : y;
}
