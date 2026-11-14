/*
 * frexp function: split x into a fraction in [0.5, 1) and a power of two.
 */
#include <math.h>

#include "xmath.h"

double (frexp)(double x, int *pexp)
{
	short binexp;

	_Dunscale(&binexp, &x);
	*pexp = binexp;
	return x;
}
