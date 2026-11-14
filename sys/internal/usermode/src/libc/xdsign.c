/*
 * _Dsign function: whether x is negative, for signbit.
 */
#include "xmath.h"

int _Dsign(double x)
{
	return DSIGN(x) != 0;
}
