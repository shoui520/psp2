/*
 * nanl function: a quiet NaN, whatever the string.
 */
#include <math.h>

#include "xmath.h"

long double (nanl)(const char *s)
{
	return _LNan._Long_double;
}
