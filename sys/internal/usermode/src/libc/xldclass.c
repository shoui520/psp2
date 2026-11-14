/*
 * _LDclass function: classify x for fpclassify.
 */
#include "xmath.h"

int _LDclass(long double x)
{
	return _LDtest(&x);
}
