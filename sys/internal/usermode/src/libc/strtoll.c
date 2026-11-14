/* strtoll function */
#include <stdlib.h>

long long strtoll(const char *__restrict s, char **__restrict endptr, int base)
{
	return _Stoll(s, endptr, base);
}
