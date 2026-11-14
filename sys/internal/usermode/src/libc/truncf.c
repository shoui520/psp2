/*
 * truncf function: x with its fraction dropped.
 */
#include <math.h>

#include "xmath.h"

float (truncf)(float x)
{
	_FDint(&x, 0);
	return x;
}
