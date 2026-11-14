/*
 * nextafterl function: the next representable value after x in the
 * direction of y.
 */
#include <math.h>

#include "xmath.h"

long double (nextafterl)(long double x, long double y)
{
	return nexttowardl(x, y);
}
