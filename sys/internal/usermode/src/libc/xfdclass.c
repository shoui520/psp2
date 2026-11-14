/*
 * _FDclass function: classify x for fpclassify, in float.
 */
#include "xmath.h"

int _FDclass(float x)
{
	return _FDtest(&x);
}
