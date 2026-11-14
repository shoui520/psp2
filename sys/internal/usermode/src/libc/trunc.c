/*
 * trunc function: x with its fraction dropped.
 */
#include <math.h>

#include "xmath.h"

double (trunc)(double x)
{
	_Dint(&x, 0);
	return x;
}
