/*
 * sinf function: the sine of x.
 */
#include <math.h>

#include "xmath.h"

float (sinf)(float x)
{
	return _FSin(x, 0);
}
