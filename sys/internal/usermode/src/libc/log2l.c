/*
 * log2l function: the base-2 logarithm of x.
 */
#include <math.h>

#include "xmath.h"

long double (log2l)(long double x)
{
	return _LLog(x, -1);
}
