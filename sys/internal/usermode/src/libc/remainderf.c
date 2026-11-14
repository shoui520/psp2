/*
 * remainderf function: x minus y times x/y rounded to nearest.
 */
#include <math.h>

#include "xmath.h"

float (remainderf)(float x, float y)
{
	return remquof(x, y, 0);
}
