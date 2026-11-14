/*
 * ceill function: the least integer not below x in long double.
 */
#include <math.h>

#include "xmath.h"

long double (ceill)(long double x)
{
	long double y = x;

	return _LDint(&y, 0) < 0 && 0.0L < x ? y + 1.0L : y;
}
