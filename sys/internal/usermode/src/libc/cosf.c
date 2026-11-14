/*
 * cosf function: the cosine in float, as the sine a quadrant on.
 */
#include <math.h>

#include "xmath.h"

float (cosf)(float x)
{
	return _FSin(x, 1);
}
