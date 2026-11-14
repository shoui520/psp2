/*
 * nan function: a quiet NaN, whatever the string.
 */
#include <math.h>

#include "xmath.h"

double (nan)(const char *s)
{
	return _Nan._Double;
}
