/* strftime function */
#include <time.h>

#include "xtime.h"

size_t strftime(char *__restrict s, size_t n, const char *__restrict fmt,
		const struct tm *__restrict t)
{
	return _Strftime(s, n, fmt, t);
}
