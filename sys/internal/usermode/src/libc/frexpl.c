/*
 * frexpl function: split x into a fraction in [0.5, 1) and a power of two in long double.
 */
#include <math.h>

#include "xmath.h"

long double (frexpl)(long double x, int *pexp)
{
	short binexp;

	_LDunscale(&binexp, &x);
	*pexp = binexp;
	return x;
}
