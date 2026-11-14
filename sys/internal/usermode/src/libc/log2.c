/*
 * log2 function: the base-2 logarithm of x.
 */
#include <math.h>

#include "xmath.h"

double (log2)(double x)
{
	return _Log(x, -1);
}
