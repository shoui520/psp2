/* strtol function */
#include <stdlib.h>

long strtol(const char *__restrict s, char **__restrict endptr, int base)
{
	return _Stolx(s, endptr, base, NULL);
}
