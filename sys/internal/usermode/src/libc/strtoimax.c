/* strtoimax function */
#include <inttypes.h>
#include <stdlib.h>

intmax_t strtoimax(const char *__restrict s, char **__restrict endptr, int base)
{
	return _Stoll(s, endptr, base);
}
