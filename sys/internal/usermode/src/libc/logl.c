/*
 * logl function: the natural logarithm of x.
 */
#include <math.h>

#include "xmath.h"

long double (logl)(long double x)
{
	return _LLog(x, 0);
}
