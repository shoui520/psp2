/* strtof function */
#include <stdlib.h>

float strtof(const char *__restrict s, char **__restrict endptr)
{
	return _Stof(s, endptr, 0);
}
