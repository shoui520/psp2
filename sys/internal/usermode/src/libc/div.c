/* div function */
#include <stdlib.h>

div_t div(int numer, int denom)
{
	div_t val;

	val.quot = numer / denom;
	val.rem = numer - denom * val.quot;
	return val;
}
