/*
 * truncl function: x with its fraction dropped.
 */
#include <math.h>

#include "xmath.h"

long double (truncl)(long double x)
{
	_LDint(&x, 0);
	return x;
}
