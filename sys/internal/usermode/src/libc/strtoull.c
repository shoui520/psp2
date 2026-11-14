/* strtoull function */
#include <stdlib.h>

unsigned long long strtoull(const char *__restrict s, char **__restrict endptr, int base)
{
	return _Stoull(s, endptr, base);
}
