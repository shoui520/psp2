/* strtoumax function */
#include <inttypes.h>
#include <stdlib.h>

uintmax_t strtoumax(const char *__restrict s, char **__restrict endptr, int base)
{
	return _Stoull(s, endptr, base);
}
