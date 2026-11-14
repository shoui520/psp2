/*
 * floorf function: the greatest integer not above x in float.
 */
#include <math.h>

#include "xmath.h"

float (floorf)(float x)
{
	float y = x;

	return _FDint(&y, 0) < 0 && x < 0.0F ? y - 1.0F : y;
}
