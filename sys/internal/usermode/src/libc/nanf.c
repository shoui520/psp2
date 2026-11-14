/*
 * nanf function: a quiet NaN, whatever the string.
 */
#include <math.h>

#include "xmath.h"

float (nanf)(const char *s)
{
	return _FNan._Float;
}
