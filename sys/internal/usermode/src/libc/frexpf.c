/*
 * frexpf function: split x into a fraction in [0.5, 1) and a power of two in float.
 */
#include <math.h>

#include "xmath.h"

float (frexpf)(float x, int *pexp)
{
	short binexp;

	_FDunscale(&binexp, &x);
	*pexp = binexp;
	return x;
}
