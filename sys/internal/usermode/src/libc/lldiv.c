/* lldiv function */
#include <stdlib.h>

lldiv_t lldiv(long long numer, long long denom)
{
	lldiv_t val;

	val.quot = numer / denom;
	val.rem = numer - denom * val.quot;
	return val;
}
