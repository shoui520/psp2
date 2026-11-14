/*
 * remainder function: x minus y times x/y rounded to nearest.
 */
#include <math.h>

#include "xmath.h"

double (remainder)(double x, double y)
{
	return remquo(x, y, 0);
}
