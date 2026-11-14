/*
 * remainderl function: x minus y times x/y rounded to nearest.
 */
#include <math.h>

#include "xmath.h"

long double (remainderl)(long double x, long double y)
{
	return remquol(x, y, 0);
}
