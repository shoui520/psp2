/*
 * log10l function: the base-10 logarithm of x.
 */
#include <math.h>

#include "xmath.h"

long double (log10l)(long double x)
{
	return _LLog(x, 1);
}
