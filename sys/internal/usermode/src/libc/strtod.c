/* strtod function */
#include <stdlib.h>

double strtod(const char *__restrict s, char **__restrict endptr)
{
	return _Stod(s, endptr, 0);
}
