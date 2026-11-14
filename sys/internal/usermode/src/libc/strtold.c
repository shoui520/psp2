/* strtold function */
#include <stdlib.h>

long double strtold(const char *__restrict s, char **__restrict endptr)
{
	return _Stold(s, endptr, 0);
}
