/*
 * log10 function: the base-10 logarithm of x.
 */
#include <math.h>

#include "xmath.h"

double (log10)(double x)
{
	return _Log(x, 1);
}
