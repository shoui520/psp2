/* strtoul function */
#include <stdlib.h>

unsigned long strtoul(const char *__restrict s, char **__restrict endptr, int base)
{
	return _Stoul(s, endptr, base);
}
