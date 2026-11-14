/*
 * ceilf function: the least integer not below x in float.
 */
#include <math.h>

#include "xmath.h"

float (ceilf)(float x)
{
	float y = x;

	return _FDint(&y, 0) < 0 && 0.0F < x ? y + 1.0F : y;
}
