/* imaxdiv function */
#include <inttypes.h>

imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom)
{
	imaxdiv_t val;

	val.quot = numer / denom;
	val.rem = numer - denom * val.quot;
	return val;
}
