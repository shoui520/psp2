/* ldiv function */
#include <stdlib.h>

ldiv_t ldiv(long numer, long denom)
{
	ldiv_t val;

	val.quot = numer / denom;
	val.rem = numer - denom * val.quot;
	return val;
}
