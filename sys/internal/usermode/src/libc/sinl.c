/*
 * sinl function: the sine of x.
 */
#include <math.h>

#include "xmath.h"

long double (sinl)(long double x)
{
	return _LSin(x, 0);
}
